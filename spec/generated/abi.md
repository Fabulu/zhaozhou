# Zhaozhou Command ABI — generated reference

GENERATED FILE - DO NOT EDIT. Source: `spec/commands.zidl` via `tools/abi-gen`
(`npm run abi:gen`). Law: `spec/capture_format.md`. Fixed-point formats:
`spec/qformats.md` (fx16 = Q16.16 in a 4-byte int32 container).

```
abi_identity_sha256 = 6b95fe820b7f9f545f1e553820208fe02e28ca77066b1df55f6af5625023373f
zidl_sha256         = a2198a6e55f7547754d5d9801a450f473609c97738e4e98684bed303988ca7b1
```

ABI version **3**, little-endian, command alignment
**16 B**, opcode width u16,
16 commands (13 implemented).

## Commands

| Command | Opcode | Record bytes | Status |
|---|---|---|---|
| `Nop` | `0x0000` | 16 | implemented |
| `BeginFrame` | `0x0001` | 32 | implemented |
| `EndFrame` | `0x0002` | 32 | implemented |
| `SetView` | `0x0010` | 96 | implemented |
| `SetPresentationContract` | `0x0020` | 48 | implemented |
| `TerrainField` | `0x0200` | 112 | implemented |
| `SurfaceStamp` | `0x0210` | 64 | implemented |
| `DrawForm` | `0x0300` | 32 | implemented |
| `DrawPopulation` | `0x0301` | 32 | implemented |
| `DrawProcedural` | `0x0302` | 64 | implemented |
| `DrawSky` | `0x0310` | 176 | reserved |
| `SetEnvironment` | `0x0311` | 48 | reserved |
| `EmitAudioEvent` | `0x0400` | 32 | implemented |
| `DebugBootstrap` | `0xF001` | 64 | reserved |
| `DebugFrameBlit` | `0xF002` | 48 | implemented |
| `DebugRumble` | `0xF004` | 32 | implemented |

Every record starts with the 16-byte command header (capture_format.md 3.1):

| Offset | Size | Field | Rule |
|---|---|---|---|
| +0 | u16 | opcode | must exist in the table above |
| +2 | u16 | record_bytes | total record size, multiple of 16, `>= 16` |
| +4 | u32 | source_id | capture_format.md 5 |
| +8 | u32 | flags | no defined bits in v1 — must be 0 |
| +12 | u32 | reserved0 | must be 0 |

### Nop — 0x0000 (16 B, implemented)

Payload bytes (offsets relative to payload start, i.e. record offset + 16):

| Offset | Size | Field | Type |
|---|---|---|---|
| — | 0 | *(no payload)* | — |

Golden sample: `tests/abi/golden/cmd_nop.bin` (C++ packer
`zhao_abi::zhao_pack_nop(zhao_abi::zhao_sample_nop(), ...)`,
TS `zhaoPackNop(zhaoSampleNop(), ...)`, SV round-trips it via
`zhao_unpack_nop`/`zhao_pack_nop`).

### BeginFrame — 0x0001 (32 B, implemented)

Payload bytes (offsets relative to payload start, i.e. record offset + 16):

| Offset | Size | Field | Type |
|---|---|---|---|
| 0 | 4 | `frame_id` | u32 |
| 4 | 4 | `resource_epoch` | u32 |
| 8 | 4 | `flags` | u32 |
| 12 | 4 | `deadline_cycles` | u32 |

Golden sample: `tests/abi/golden/cmd_begin_frame.bin` (C++ packer
`zhao_abi::zhao_pack_begin_frame(zhao_abi::zhao_sample_begin_frame(), ...)`,
TS `zhaoPackBeginFrame(zhaoSampleBeginFrame(), ...)`, SV round-trips it via
`zhao_unpack_begin_frame`/`zhao_pack_begin_frame`).

### EndFrame — 0x0002 (32 B, implemented)

Payload bytes (offsets relative to payload start, i.e. record offset + 16):

| Offset | Size | Field | Type |
|---|---|---|---|
| 0 | 4 | `completion_flags` | u32 |
| 4 | 4 | `expected_crc_valid` | u32 |
| 8 | 4 | `expected_framebuffer_crc` | u32 |
| 12 | 4 | `pad` | pad (zero) ×4 |

Golden sample: `tests/abi/golden/cmd_end_frame.bin` (C++ packer
`zhao_abi::zhao_pack_end_frame(zhao_abi::zhao_sample_end_frame(), ...)`,
TS `zhaoPackEndFrame(zhaoSampleEndFrame(), ...)`, SV round-trips it via
`zhao_unpack_end_frame`/`zhao_pack_end_frame`).

### SetView — 0x0010 (96 B, implemented)

Payload bytes (offsets relative to payload start, i.e. record offset + 16):

| Offset | Size | Field | Type |
|---|---|---|---|
| 0 | 1 | `view_id` | u8 |
| 1 | 1 | `viewport_id` | u8 |
| 2 | 2 | `flags` | u16 |
| 4 | 64 | `view_projection` | mat4fx |
| 68 | 4 | `pixel_error` | fx16 |
| 72 | 4 | `geometry_tokens` | u32 |
| 76 | 4 | `fragment_tokens` | u32 |

`view_projection` (mat4fx) leaves:

| Offset | Size | Leaf | Type |
|---|---|---|---|
| 8 | 4 | `view_projection.m00` | fx16 |
| 12 | 4 | `view_projection.m01` | fx16 |
| 16 | 4 | `view_projection.m02` | fx16 |
| 20 | 4 | `view_projection.m03` | fx16 |
| 24 | 4 | `view_projection.m10` | fx16 |
| 28 | 4 | `view_projection.m11` | fx16 |
| 32 | 4 | `view_projection.m12` | fx16 |
| 36 | 4 | `view_projection.m13` | fx16 |
| 40 | 4 | `view_projection.m20` | fx16 |
| 44 | 4 | `view_projection.m21` | fx16 |
| 48 | 4 | `view_projection.m22` | fx16 |
| 52 | 4 | `view_projection.m23` | fx16 |
| 56 | 4 | `view_projection.m30` | fx16 |
| 60 | 4 | `view_projection.m31` | fx16 |
| 64 | 4 | `view_projection.m32` | fx16 |
| 68 | 4 | `view_projection.m33` | fx16 |

Golden sample: `tests/abi/golden/cmd_set_view.bin` (C++ packer
`zhao_abi::zhao_pack_set_view(zhao_abi::zhao_sample_set_view(), ...)`,
TS `zhaoPackSetView(zhaoSampleSetView(), ...)`, SV round-trips it via
`zhao_unpack_set_view`/`zhao_pack_set_view`).

### SetPresentationContract — 0x0020 (48 B, implemented)

Payload bytes (offsets relative to payload start, i.e. record offset + 16):

| Offset | Size | Field | Type |
|---|---|---|---|
| 0 | 1 | `mode` | video_mode |
| 1 | 1 | `view_count` | u8 |
| 2 | 2 | `flags` | u16 |
| 4 | 8 | `geometry_tokens` | u32 ×2 |
| 12 | 8 | `fragment_tokens` | u32 ×2 |
| 20 | 4 | `shared_tokens` | u32 |
| 24 | 8 | `pad` | pad (zero) ×8 |

Golden sample: `tests/abi/golden/cmd_set_presentation_contract.bin` (C++ packer
`zhao_abi::zhao_pack_set_presentation_contract(zhao_abi::zhao_sample_set_presentation_contract(), ...)`,
TS `zhaoPackSetPresentationContract(zhaoSampleSetPresentationContract(), ...)`, SV round-trips it via
`zhao_unpack_set_presentation_contract`/`zhao_pack_set_presentation_contract`).

### TerrainField — 0x0200 (112 B, implemented)

Payload bytes (offsets relative to payload start, i.e. record offset + 16):

| Offset | Size | Field | Type |
|---|---|---|---|
| 0 | 4 | `program` | handle32 [program] |
| 4 | 16 | `footprint` | rectfx |
| 20 | 4 | `start_tick` | u32 |
| 24 | 4 | `duration_ticks` | u32 |
| 28 | 64 | `parameters` | u8 ×64 |
| 92 | 4 | `pad` | pad (zero) ×4 |

`footprint` (rectfx) leaves:

| Offset | Size | Leaf | Type |
|---|---|---|---|
| 8 | 4 | `footprint.x0` | fx16 |
| 12 | 4 | `footprint.y0` | fx16 |
| 16 | 4 | `footprint.x1` | fx16 |
| 20 | 4 | `footprint.y1` | fx16 |

Golden sample: `tests/abi/golden/cmd_terrain_field.bin` (C++ packer
`zhao_abi::zhao_pack_terrain_field(zhao_abi::zhao_sample_terrain_field(), ...)`,
TS `zhaoPackTerrainField(zhaoSampleTerrainField(), ...)`, SV round-trips it via
`zhao_unpack_terrain_field`/`zhao_pack_terrain_field`).

### SurfaceStamp — 0x0210 (64 B, implemented)

Payload bytes (offsets relative to payload start, i.e. record offset + 16):

| Offset | Size | Field | Type |
|---|---|---|---|
| 0 | 4 | `brush` | handle32 [brush] |
| 4 | 4 | `patch` | handle32 [patch] |
| 8 | 1 | `operation` | u8 |
| 9 | 1 | `tag` | u8 |
| 10 | 2 | `strength` | u16 |
| 12 | 24 | `transform` | transform2fx |
| 36 | 4 | `radius` | fx16 |
| 40 | 4 | `ring_width` | fx16 |
| 44 | 4 | `pad` | pad (zero) ×4 |

`transform` (transform2fx) leaves:

| Offset | Size | Leaf | Type |
|---|---|---|---|
| 24 | 4 | `transform.tx` | fx16 |
| 28 | 4 | `transform.ty` | fx16 |
| 32 | 4 | `transform.r00` | fx16 |
| 36 | 4 | `transform.r01` | fx16 |
| 40 | 4 | `transform.r10` | fx16 |
| 44 | 4 | `transform.r11` | fx16 |

Golden sample: `tests/abi/golden/cmd_surface_stamp.bin` (C++ packer
`zhao_abi::zhao_pack_surface_stamp(zhao_abi::zhao_sample_surface_stamp(), ...)`,
TS `zhaoPackSurfaceStamp(zhaoSampleSurfaceStamp(), ...)`, SV round-trips it via
`zhao_unpack_surface_stamp`/`zhao_pack_surface_stamp`).

### DrawForm — 0x0300 (32 B, implemented)

Payload bytes (offsets relative to payload start, i.e. record offset + 16):

| Offset | Size | Field | Type |
|---|---|---|---|
| 0 | 4 | `form` | handle32 [form] |
| 4 | 4 | `material_set` | handle32 [material_set] |
| 8 | 4 | `transform` | handle32 [transform] |
| 12 | 1 | `viewport_mask` | u8 |
| 13 | 1 | `semantic_weight` | u8 |
| 14 | 2 | `flags` | u16 |

Golden sample: `tests/abi/golden/cmd_draw_form.bin` (C++ packer
`zhao_abi::zhao_pack_draw_form(zhao_abi::zhao_sample_draw_form(), ...)`,
TS `zhaoPackDrawForm(zhaoSampleDrawForm(), ...)`, SV round-trips it via
`zhao_unpack_draw_form`/`zhao_pack_draw_form`).

### DrawPopulation — 0x0301 (32 B, implemented)

Payload bytes (offsets relative to payload start, i.e. record offset + 16):

| Offset | Size | Field | Type |
|---|---|---|---|
| 0 | 4 | `population` | handle32 [population] |
| 4 | 1 | `viewport_mask` | u8 |
| 5 | 1 | `semantic_weight` | u8 |
| 6 | 2 | `flags` | u16 |
| 8 | 8 | `pad` | pad (zero) ×8 |

Golden sample: `tests/abi/golden/cmd_draw_population.bin` (C++ packer
`zhao_abi::zhao_pack_draw_population(zhao_abi::zhao_sample_draw_population(), ...)`,
TS `zhaoPackDrawPopulation(zhaoSampleDrawPopulation(), ...)`, SV round-trips it via
`zhao_unpack_draw_population`/`zhao_pack_draw_population`).

### DrawProcedural — 0x0302 (64 B, implemented)

Payload bytes (offsets relative to payload start, i.e. record offset + 16):

| Offset | Size | Field | Type |
|---|---|---|---|
| 0 | 4 | `program` | handle32 [forge_program] |
| 4 | 4 | `material` | handle32 [material] |
| 8 | 24 | `transform` | transform2fx |
| 32 | 4 | `screen_error` | fx16 |
| 36 | 1 | `kind` | forge_kind |
| 37 | 11 | `pad` | pad (zero) ×11 |

`transform` (transform2fx) leaves:

| Offset | Size | Leaf | Type |
|---|---|---|---|
| 16 | 4 | `transform.tx` | fx16 |
| 20 | 4 | `transform.ty` | fx16 |
| 24 | 4 | `transform.r00` | fx16 |
| 28 | 4 | `transform.r01` | fx16 |
| 32 | 4 | `transform.r10` | fx16 |
| 36 | 4 | `transform.r11` | fx16 |

Golden sample: `tests/abi/golden/cmd_draw_procedural.bin` (C++ packer
`zhao_abi::zhao_pack_draw_procedural(zhao_abi::zhao_sample_draw_procedural(), ...)`,
TS `zhaoPackDrawProcedural(zhaoSampleDrawProcedural(), ...)`, SV round-trips it via
`zhao_unpack_draw_procedural`/`zhao_pack_draw_procedural`).

### DrawSky — 0x0310 (176 B, reserved)

Payload bytes (offsets relative to payload start, i.e. record offset + 16):

| Offset | Size | Field | Type |
|---|---|---|---|
| 0 | 4 | `sky_set` | handle32 [sky_set] |
| 4 | 128 | `rot_proj` | mat4fx ×2 |
| 132 | 4 | `cloud_scroll_u` | fx16 |
| 136 | 4 | `cloud_scroll_v` | fx16 |
| 140 | 2 | `drum_yaw` | angle16 |
| 142 | 1 | `viewport_mask` | u8 |
| 143 | 1 | `flags` | u8 |
| 144 | 1 | `reserved0` | u8 |
| 145 | 1 | `reserved1` | u8 |
| 146 | 14 | `pad` | pad (zero) ×14 |

`rot_proj` (mat4fx) leaves:

| Offset | Size | Leaf | Type |
|---|---|---|---|
| 8 | 4 | `rot_proj[0].m00` | fx16 |
| 12 | 4 | `rot_proj[0].m01` | fx16 |
| 16 | 4 | `rot_proj[0].m02` | fx16 |
| 20 | 4 | `rot_proj[0].m03` | fx16 |
| 24 | 4 | `rot_proj[0].m10` | fx16 |
| 28 | 4 | `rot_proj[0].m11` | fx16 |
| 32 | 4 | `rot_proj[0].m12` | fx16 |
| 36 | 4 | `rot_proj[0].m13` | fx16 |
| 40 | 4 | `rot_proj[0].m20` | fx16 |
| 44 | 4 | `rot_proj[0].m21` | fx16 |
| 48 | 4 | `rot_proj[0].m22` | fx16 |
| 52 | 4 | `rot_proj[0].m23` | fx16 |
| 56 | 4 | `rot_proj[0].m30` | fx16 |
| 60 | 4 | `rot_proj[0].m31` | fx16 |
| 64 | 4 | `rot_proj[0].m32` | fx16 |
| 68 | 4 | `rot_proj[0].m33` | fx16 |
| 72 | 4 | `rot_proj[1].m00` | fx16 |
| 76 | 4 | `rot_proj[1].m01` | fx16 |
| 80 | 4 | `rot_proj[1].m02` | fx16 |
| 84 | 4 | `rot_proj[1].m03` | fx16 |
| 88 | 4 | `rot_proj[1].m10` | fx16 |
| 92 | 4 | `rot_proj[1].m11` | fx16 |
| 96 | 4 | `rot_proj[1].m12` | fx16 |
| 100 | 4 | `rot_proj[1].m13` | fx16 |
| 104 | 4 | `rot_proj[1].m20` | fx16 |
| 108 | 4 | `rot_proj[1].m21` | fx16 |
| 112 | 4 | `rot_proj[1].m22` | fx16 |
| 116 | 4 | `rot_proj[1].m23` | fx16 |
| 120 | 4 | `rot_proj[1].m30` | fx16 |
| 124 | 4 | `rot_proj[1].m31` | fx16 |
| 128 | 4 | `rot_proj[1].m32` | fx16 |
| 132 | 4 | `rot_proj[1].m33` | fx16 |

Golden sample: `tests/abi/golden/cmd_draw_sky.bin` (C++ packer
`zhao_abi::zhao_pack_draw_sky(zhao_abi::zhao_sample_draw_sky(), ...)`,
TS `zhaoPackDrawSky(zhaoSampleDrawSky(), ...)`, SV round-trips it via
`zhao_unpack_draw_sky`/`zhao_pack_draw_sky`).

### SetEnvironment — 0x0311 (48 B, reserved)

Payload bytes (offsets relative to payload start, i.e. record offset + 16):

| Offset | Size | Field | Type |
|---|---|---|---|
| 0 | 2 | `sun_yaw` | angle16 |
| 2 | 2 | `sun_pitch` | angle16 |
| 4 | 2 | `sun_colour` | rgb565 |
| 6 | 2 | `ambient` | rgb565 |
| 8 | 2 | `tint` | rgb565 |
| 10 | 1 | `tint_strength` | u8 |
| 11 | 1 | `fog` | fog_mode |
| 12 | 4 | `fog_near` | fx16 |
| 16 | 4 | `fog_far` | fx16 |
| 20 | 12 | `pad` | pad (zero) ×12 |

`sun_colour` (rgb565) leaves:

| Offset | Size | Leaf | Type |
|---|---|---|---|
| 8 | 2 | `sun_colour.bits` | u16 |

`ambient` (rgb565) leaves:

| Offset | Size | Leaf | Type |
|---|---|---|---|
| 12 | 2 | `ambient.bits` | u16 |

`tint` (rgb565) leaves:

| Offset | Size | Leaf | Type |
|---|---|---|---|
| 16 | 2 | `tint.bits` | u16 |

Golden sample: `tests/abi/golden/cmd_set_environment.bin` (C++ packer
`zhao_abi::zhao_pack_set_environment(zhao_abi::zhao_sample_set_environment(), ...)`,
TS `zhaoPackSetEnvironment(zhaoSampleSetEnvironment(), ...)`, SV round-trips it via
`zhao_unpack_set_environment`/`zhao_pack_set_environment`).

### EmitAudioEvent — 0x0400 (32 B, implemented)

Payload bytes (offsets relative to payload start, i.e. record offset + 16):

| Offset | Size | Field | Type |
|---|---|---|---|
| 0 | 4 | `event_id` | u32 |
| 4 | 2 | `pan_fx` | i16 |
| 6 | 2 | `gain` | u16 |
| 8 | 4 | `sample_handle` | u32 |
| 12 | 4 | `timestamp` | u32 |

Golden sample: `tests/abi/golden/cmd_emit_audio_event.bin` (C++ packer
`zhao_abi::zhao_pack_emit_audio_event(zhao_abi::zhao_sample_emit_audio_event(), ...)`,
TS `zhaoPackEmitAudioEvent(zhaoSampleEmitAudioEvent(), ...)`, SV round-trips it via
`zhao_unpack_emit_audio_event`/`zhao_pack_emit_audio_event`).

### DebugBootstrap — 0xF001 (64 B, reserved)

Payload bytes (offsets relative to payload start, i.e. record offset + 16):

| Offset | Size | Field | Type |
|---|---|---|---|
| 0 | 48 | `data` | u8 ×48 |

Golden sample: `tests/abi/golden/cmd_debug_bootstrap.bin` (C++ packer
`zhao_abi::zhao_pack_debug_bootstrap(zhao_abi::zhao_sample_debug_bootstrap(), ...)`,
TS `zhaoPackDebugBootstrap(zhaoSampleDebugBootstrap(), ...)`, SV round-trips it via
`zhao_unpack_debug_bootstrap`/`zhao_pack_debug_bootstrap`).

### DebugFrameBlit — 0xF002 (48 B, implemented)

Payload bytes (offsets relative to payload start, i.e. record offset + 16):

| Offset | Size | Field | Type |
|---|---|---|---|
| 0 | 1 | `dst_slot` | u8 |
| 1 | 1 | `mode` | video_mode |
| 2 | 2 | `pad` | pad (zero) ×2 |
| 4 | 4 | `src_addr_hps` | u32 |
| 8 | 4 | `byte_len` | u32 |
| 12 | 4 | `expected_crc32c` | u32 |
| 16 | 16 | `pad_1` | pad (zero) ×16 |

Golden sample: `tests/abi/golden/cmd_debug_frame_blit.bin` (C++ packer
`zhao_abi::zhao_pack_debug_frame_blit(zhao_abi::zhao_sample_debug_frame_blit(), ...)`,
TS `zhaoPackDebugFrameBlit(zhaoSampleDebugFrameBlit(), ...)`, SV round-trips it via
`zhao_unpack_debug_frame_blit`/`zhao_pack_debug_frame_blit`).

### DebugRumble — 0xF004 (32 B, implemented)

Payload bytes (offsets relative to payload start, i.e. record offset + 16):

| Offset | Size | Field | Type |
|---|---|---|---|
| 0 | 1 | `pad_index` | u8 |
| 1 | 1 | `enable` | u8 |
| 2 | 1 | `strength` | u8 |
| 3 | 13 | `pad` | pad (zero) ×13 |

Golden sample: `tests/abi/golden/cmd_debug_rumble.bin` (C++ packer
`zhao_abi::zhao_pack_debug_rumble(zhao_abi::zhao_sample_debug_rumble(), ...)`,
TS `zhaoPackDebugRumble(zhaoSampleDebugRumble(), ...)`, SV round-trips it via
`zhao_unpack_debug_rumble`/`zhao_pack_debug_rumble`).

## Composed structs

### rectfx — 16 B

| Offset | Size | Field | Type |
|---|---|---|---|
| 0 | 4 | `x0` | fx16 |
| 4 | 4 | `y0` | fx16 |
| 8 | 4 | `x1` | fx16 |
| 12 | 4 | `y1` | fx16 |

### transform2fx — 24 B

| Offset | Size | Field | Type |
|---|---|---|---|
| 0 | 4 | `tx` | fx16 |
| 4 | 4 | `ty` | fx16 |
| 8 | 4 | `r00` | fx16 |
| 12 | 4 | `r01` | fx16 |
| 16 | 4 | `r10` | fx16 |
| 20 | 4 | `r11` | fx16 |

### mat4fx — 64 B

| Offset | Size | Field | Type |
|---|---|---|---|
| 0 | 4 | `m00` | fx16 |
| 4 | 4 | `m01` | fx16 |
| 8 | 4 | `m02` | fx16 |
| 12 | 4 | `m03` | fx16 |
| 16 | 4 | `m10` | fx16 |
| 20 | 4 | `m11` | fx16 |
| 24 | 4 | `m12` | fx16 |
| 28 | 4 | `m13` | fx16 |
| 32 | 4 | `m20` | fx16 |
| 36 | 4 | `m21` | fx16 |
| 40 | 4 | `m22` | fx16 |
| 44 | 4 | `m23` | fx16 |
| 48 | 4 | `m30` | fx16 |
| 52 | 4 | `m31` | fx16 |
| 56 | 4 | `m32` | fx16 |
| 60 | 4 | `m33` | fx16 |

### rgb565 — 2 B

| Offset | Size | Field | Type |
|---|---|---|---|
| 0 | 2 | `bits` | u16 |

### PadFrame — 20 B

| Offset | Size | Field | Type |
|---|---|---|---|
| 0 | 1 | `pad_index` | u8 |
| 1 | 1 | `flags` | u8 |
| 2 | 2 | `sequence` | u16 |
| 4 | 4 | `buttons` | u32 |
| 8 | 2 | `lx` | i16 |
| 10 | 2 | `ly` | i16 |
| 12 | 2 | `rx` | i16 |
| 14 | 2 | `ry` | i16 |
| 16 | 4 | `rsv` | u32 |

## Value enums (ABI v2)

### `video_mode` — backing `u8`

| Value | Name |
|---|---|
| 0 | `VIDEO_Z60` |
| 1 | `VIDEO_STORM` |
| 2 | `VIDEO_DUO` |

### `forge_kind` — backing `u8`

| Value | Name |
|---|---|
| 0 | `FORGE_HEIGHTFIELD_PATCH` |

### `fog_mode` — backing `u8`

| Value | Name |
|---|---|
| 0 | `FOG_OFF` |
| 1 | `FOG_LINEAR` |

## Error codes (generated enum, shared verbatim C++/TS/SV)

| Value | Name | Meaning |
|---|---|---|
| 0 | `ZH_ABI_OK` | |
| 1 | `ZH_ABI_BAD_MAGIC` | |
| 2 | `ZH_ABI_BAD_ABI_VERSION` | |
| 3 | `ZH_ABI_RESERVED_FLAG` | |
| 4 | `ZH_ABI_BAD_LENGTH` | |
| 5 | `ZH_ABI_BAD_HEADER_CRC` | |
| 6 | `ZH_ABI_BAD_PAYLOAD_CRC` | |
| 7 | `ZH_ABI_UNKNOWN_OPCODE` | |
| 8 | `ZH_ABI_RESERVED_FIELD` | |
| 9 | `ZH_ABI_BAD_VALUE` | |
| 10 | `ZH_ABI_STALE_HANDLE` | |
| 11 | `ZH_ABI_TRUNCATED` | |
| 12 | `ZH_ABI_DEBUG_FLAG_REQUIRED` | |
| 13 | `ZH_ABI_COUNT_MISMATCH` | |
| 14 | `ZH_ABI_UNIMPLEMENTED_COMMAND` | |

## Frame packet

See `spec/capture_format.md` 3. 36-byte sealed header + command stream
+ trailing payload CRC-32C; total `40 + command_bytes`; slot bounded by
`FRAME_SLOT_BYTES = 1048576`.

## Golden binaries (committed evidence, regenerated by `npm run abi:gen`)

| File | Contents |
|---|---|
| `tests/abi/golden/cmd_nop.bin` | canonical Nop sample record |
| `tests/abi/golden/cmd_begin_frame.bin` | canonical BeginFrame sample record |
| `tests/abi/golden/cmd_end_frame.bin` | canonical EndFrame sample record |
| `tests/abi/golden/cmd_set_view.bin` | canonical SetView sample record |
| `tests/abi/golden/cmd_set_presentation_contract.bin` | canonical SetPresentationContract sample record |
| `tests/abi/golden/cmd_terrain_field.bin` | canonical TerrainField sample record |
| `tests/abi/golden/cmd_surface_stamp.bin` | canonical SurfaceStamp sample record |
| `tests/abi/golden/cmd_draw_form.bin` | canonical DrawForm sample record |
| `tests/abi/golden/cmd_draw_population.bin` | canonical DrawPopulation sample record |
| `tests/abi/golden/cmd_draw_procedural.bin` | canonical DrawProcedural sample record |
| `tests/abi/golden/cmd_draw_sky.bin` | canonical DrawSky sample record |
| `tests/abi/golden/cmd_set_environment.bin` | canonical SetEnvironment sample record |
| `tests/abi/golden/cmd_emit_audio_event.bin` | canonical EmitAudioEvent sample record |
| `tests/abi/golden/cmd_debug_bootstrap.bin` | canonical DebugBootstrap sample record |
| `tests/abi/golden/cmd_debug_frame_blit.bin` | canonical DebugFrameBlit sample record |
| `tests/abi/golden/cmd_debug_rumble.bin` | canonical DebugRumble sample record |
| `tests/abi/golden/frame_minimal.bin` | BeginFrame/Nop/EndFrame sealed packet |
| `tests/abi/golden/zcap_minimal.zcap` | minimal .zcap (ABI_INFO + FRAME_PACKET + SOURCE_MAP) |
| `tests/abi/golden/abi_corpus.zcorpus` | fuzz corpus with expected error codes |
