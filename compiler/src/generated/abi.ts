// GENERATED FILE - DO NOT EDIT
// Source: spec/commands.zidl via tools/abi-gen (`npm run abi:gen`).
// Law: spec/capture_format.md. Identity (see spec/generated/abi.md):
//   abi_identity_sha256 = 06b5df38e6c838560bc66f7fe8e3359965c1fa87b91b90b91a318cd50373edae
//   zidl_sha256         = 8dd155e8e7eabd5926a636b14ca241bbe6b67c32afca3b6822cdb7026328cbda

// ---------------------------------------------------------------- abi ---

export const ZHAO_ABI_VERSION = 4 as const;
export const ZHAO_COMMAND_ALIGNMENT = 16 as const;
export const FRAME_SLOT_BYTES = 1048576 as const;
export const QFMT_VERSION = 3 as const;
export const RESOURCE_KIND_TWOD_PAGE = 15 as const;

// error codes — shared verbatim across C++/TS/SV (zhao_abi.h / zhao_abi_pkg.sv)
export const ZH_ABI_OK = 0 as const;
export const ZH_ABI_BAD_MAGIC = 1 as const;
export const ZH_ABI_BAD_ABI_VERSION = 2 as const;
export const ZH_ABI_RESERVED_FLAG = 3 as const;
export const ZH_ABI_BAD_LENGTH = 4 as const;
export const ZH_ABI_BAD_HEADER_CRC = 5 as const;
export const ZH_ABI_BAD_PAYLOAD_CRC = 6 as const;
export const ZH_ABI_UNKNOWN_OPCODE = 7 as const;
export const ZH_ABI_RESERVED_FIELD = 8 as const;
export const ZH_ABI_BAD_VALUE = 9 as const;
export const ZH_ABI_STALE_HANDLE = 10 as const;
export const ZH_ABI_TRUNCATED = 11 as const;
export const ZH_ABI_DEBUG_FLAG_REQUIRED = 12 as const;
export const ZH_ABI_COUNT_MISMATCH = 13 as const;
export const ZH_ABI_UNIMPLEMENTED_COMMAND = 14 as const;
export const ZHAO_ERROR_NAMES: Record<number, string> = {
  [0]: 'ZH_ABI_OK',
  [1]: 'ZH_ABI_BAD_MAGIC',
  [2]: 'ZH_ABI_BAD_ABI_VERSION',
  [3]: 'ZH_ABI_RESERVED_FLAG',
  [4]: 'ZH_ABI_BAD_LENGTH',
  [5]: 'ZH_ABI_BAD_HEADER_CRC',
  [6]: 'ZH_ABI_BAD_PAYLOAD_CRC',
  [7]: 'ZH_ABI_UNKNOWN_OPCODE',
  [8]: 'ZH_ABI_RESERVED_FIELD',
  [9]: 'ZH_ABI_BAD_VALUE',
  [10]: 'ZH_ABI_STALE_HANDLE',
  [11]: 'ZH_ABI_TRUNCATED',
  [12]: 'ZH_ABI_DEBUG_FLAG_REQUIRED',
  [13]: 'ZH_ABI_COUNT_MISMATCH',
  [14]: 'ZH_ABI_UNIMPLEMENTED_COMMAND',
};

// enum video_mode: u8 on the wire (capture_format.md 3.2 step 7)
export const VIDEO_Z60 = 0 as const;
export const VIDEO_STORM = 1 as const;
export const VIDEO_DUO = 2 as const;
export const ZHAO_ENUM_VIDEO_MODE: readonly number[] = [0, 1, 2];

// enum forge_kind: u8 on the wire (capture_format.md 3.2 step 7)
export const FORGE_HEIGHTFIELD_PATCH = 0 as const;
export const FORGE_RIBBON = 1 as const;
export const FORGE_RADIAL_FAN = 2 as const;
export const FORGE_TUBE = 3 as const;
export const FORGE_RADIAL_SHELL = 4 as const;
export const FORGE_BILLBOARD_SHEET = 5 as const;
export const ZHAO_ENUM_FORGE_KIND: readonly number[] = [0, 1, 2, 3, 4, 5];

// enum fog_mode: u8 on the wire (capture_format.md 3.2 step 7)
export const FOG_OFF = 0 as const;
export const FOG_LINEAR = 1 as const;
export const ZHAO_ENUM_FOG_MODE: readonly number[] = [0, 1];

// enum warp_attribute_mode: u8 on the wire (capture_format.md 3.2 step 7)
export const WARP_ATTR_INLINE4 = 0 as const;
export const WARP_ATTR_STREAM4 = 1 as const;
export const ZHAO_ENUM_WARP_ATTRIBUTE_MODE: readonly number[] = [0, 1];

// opcodes
export const ZHAO_OP_NOP = 0x0000; // 16 B, implemented
export const ZHAO_OP_BEGIN_FRAME = 0x0001; // 32 B, implemented
export const ZHAO_OP_SEAL_FRAME_PLAN = 0x0003; // 48 B, implemented
export const ZHAO_OP_END_FRAME = 0x0002; // 32 B, implemented
export const ZHAO_OP_SET_VIEW = 0x0010; // 112 B, implemented
export const ZHAO_OP_SET_PRESENTATION_CONTRACT = 0x0020; // 48 B, implemented
export const ZHAO_OP_TERRAIN_FIELD = 0x0200; // 112 B, implemented
export const ZHAO_OP_SURFACE_STAMP = 0x0210; // 64 B, implemented
export const ZHAO_OP_TERRAIN_EPOCH = 0x0220; // 32 B, reserved
export const ZHAO_OP_SUBMIT_TERRAIN_SET = 0x0230; // 48 B, reserved
export const ZHAO_OP_DRAW_FORM = 0x0300; // 32 B, implemented
export const ZHAO_OP_DRAW_POPULATION = 0x0301; // 32 B, implemented
export const ZHAO_OP_DRAW_PROCEDURAL = 0x0302; // 64 B, implemented
export const ZHAO_OP_DRAW_SKY = 0x0310; // 176 B, reserved
export const ZHAO_OP_SET_ENVIRONMENT = 0x0311; // 48 B, implemented
export const ZHAO_OP_EMIT_AUDIO_EVENT = 0x0400; // 32 B, implemented
export const ZHAO_OP_DEBUG_BOOTSTRAP = 0xF001; // 64 B, reserved
export const ZHAO_OP_DEBUG_FRAME_BLIT = 0xF002; // 48 B, implemented
export const ZHAO_OP_DEBUG_RUMBLE = 0xF004; // 32 B, implemented
export const ZHAO_OP_PUBLISH_RESOURCE = 0x0030; // 48 B, implemented
export const ZHAO_OP_SET_POST = 0x0040; // 32 B, implemented
export const ZHAO_OP_SET_GRADE_TABLE = 0x0041; // 96 B, implemented
export const ZHAO_OP_SET_POPULATION = 0x0303; // 48 B, implemented
export const ZHAO_OP_DEBUG_TRACE_ARM = 0xF003; // 32 B, implemented
export const ZHAO_OP_DRAW_POSED_FORM = 0x0305; // 48 B, implemented
export const ZHAO_OP_DRAW_WARPED_FORM = 0x0304; // 96 B, implemented
export const ZHAO_OP_SET_PLANE = 0x0306; // 64 B, implemented
export const ZHAO_OP_DRAW_SPRITE = 0x0307; // 64 B, implemented

// frame packet (capture_format.md 3)
export const ZHAO_FRAME_MAGIC = 0x314b505a; // 'Z','P','K','1' LE
export const ZHAO_FRAME_HEADER_BYTES = 36;
export const ZHAO_FRAME_OVERHEAD = 40;
export const ZHAO_FRAME_FLAG_CONTAINS_DEBUG = 0x0001;
export const ZHAO_COMPL_DONE = 0x01;
export const ZHAO_COMPL_ERR = 0x02;
export const ZHAO_OFF_MAGIC = 0 as const;
export const ZHAO_OFF_ABI_VERSION = 4 as const;
export const ZHAO_OFF_FLAGS = 6 as const;
export const ZHAO_OFF_FRAME_ID = 8 as const;
export const ZHAO_OFF_SEQUENCE = 12 as const;
export const ZHAO_OFF_RESOURCE_EPOCH = 16 as const;
export const ZHAO_OFF_DEADLINE = 20 as const;
export const ZHAO_OFF_COMMAND_COUNT = 24 as const;
export const ZHAO_OFF_COMMAND_BYTES = 28 as const;
export const ZHAO_OFF_HEADER_CRC = 32 as const;

// source-id scheme (capture_format.md 5)
export const ZHAO_SOURCE_KIND_NONE = 0;
export const ZHAO_SOURCE_KIND_COMMAND_SITE = 5;
export function zhaoSourceIdEncode(kind: number, module: number, index: number): number {
  return ((kind << 28) | (module << 16) | index) >>> 0;
}
export function zhaoSourceIdDecode(id: number): { kind: number; module: number; index: number } {
  return { kind: id >>> 28, module: (id >>> 16) & 0xfff, index: id & 0xffff };
}

/** rectfx: 16 bytes (spec/commands.zidl); pads are not modeled */
export interface ZhRectfx {
  x0: number; // fx16 (Q16.16, int32), @0
  y0: number; // fx16 (Q16.16, int32), @4
  x1: number; // fx16 (Q16.16, int32), @8
  y1: number; // fx16 (Q16.16, int32), @12
}

/** transform2fx: 24 bytes (spec/commands.zidl); pads are not modeled */
export interface ZhTransform2fx {
  tx: number; // fx16 (Q16.16, int32), @0
  ty: number; // fx16 (Q16.16, int32), @4
  r00: number; // fx16 (Q16.16, int32), @8
  r01: number; // fx16 (Q16.16, int32), @12
  r10: number; // fx16 (Q16.16, int32), @16
  r11: number; // fx16 (Q16.16, int32), @20
}

/** mat4fx: 64 bytes (spec/commands.zidl); pads are not modeled */
export interface ZhMat4fx {
  m00: number; // fx16 (Q16.16, int32), @0
  m01: number; // fx16 (Q16.16, int32), @4
  m02: number; // fx16 (Q16.16, int32), @8
  m03: number; // fx16 (Q16.16, int32), @12
  m10: number; // fx16 (Q16.16, int32), @16
  m11: number; // fx16 (Q16.16, int32), @20
  m12: number; // fx16 (Q16.16, int32), @24
  m13: number; // fx16 (Q16.16, int32), @28
  m20: number; // fx16 (Q16.16, int32), @32
  m21: number; // fx16 (Q16.16, int32), @36
  m22: number; // fx16 (Q16.16, int32), @40
  m23: number; // fx16 (Q16.16, int32), @44
  m30: number; // fx16 (Q16.16, int32), @48
  m31: number; // fx16 (Q16.16, int32), @52
  m32: number; // fx16 (Q16.16, int32), @56
  m33: number; // fx16 (Q16.16, int32), @60
}

/** rgb565: 2 bytes (spec/commands.zidl); pads are not modeled */
export interface ZhRgb565 {
  bits: number; // u16, @0
}

/** PadFrame: 20 bytes (spec/commands.zidl); pads are not modeled */
export interface ZhPadFrame {
  pad_index: number; // u8, @0
  flags: number; // u8, @1
  sequence: number; // u16, @2
  buttons: number; // u32, @4
  lx: number; // i16, @8
  ly: number; // i16, @10
  rx: number; // i16, @12
  ry: number; // i16, @14
  rsv: number; // u32, @16
}

/** MaterialSample: 4 bytes (spec/commands.zidl); pads are not modeled */
export interface ZhMaterialSample {
  binding_slot: number; // u16, @0
  binding_generation: number; // u8, @2
  modes: number; // u8, @3
}

/** MaterialRecord: 32 bytes (spec/commands.zidl); pads are not modeled */
export interface ZhMaterialRecord {
  control: number; // u8, @0
  recipe_weight: number; // u8, @1
  flags: number; // u16, @2
  sample0: ZhMaterialSample; // @4
  sample1: ZhMaterialSample; // @8
  sample2: ZhMaterialSample; // @12
  palette_base: number; // u32, @16
  raster_state: number; // u32, @20
  fragment_state: number; // u32, @24
  fragment_decl: number; // u32, @28
}

/** 16-byte command record header (capture_format.md 3.1) */
export interface ZhCmdHeader {
  opcode: number;
  recordBytes: number;
  sourceId: number;
  flags: number; // no defined bits in v1 -> must be 0
}

/** Nop 0x0000: 16-byte record (implemented) */
export interface ZhRecordNop {
  hdr: ZhCmdHeader;
}

/** BeginFrame 0x0001: 32-byte record (implemented) */
export interface ZhRecordBeginFrame {
  hdr: ZhCmdHeader;
  frame_id: number; // u32, @0
  resource_epoch: number; // u32, @4
  flags: number; // u32, @8
  deadline_cycles: number; // u32, @12
}

/** SealFramePlan 0x0003: 48-byte record (implemented) */
export interface ZhRecordSealFramePlan {
  hdr: ZhCmdHeader;
  view_id: number; // u8, @0
  flags: number; // u8, @1
  resource_gen: number; // u16, @2
  view_gen: number; // u16, @4
  giant_instance: number; // u16, @6
  plan_verts: number; // u32, @8
  plan_tris: number; // u32, @12
  plan_chunks: number; // u32, @16
  plan_refs: number; // u32, @20
  giant_refs: number; // u32, @24
}

/** EndFrame 0x0002: 32-byte record (implemented) */
export interface ZhRecordEndFrame {
  hdr: ZhCmdHeader;
  completion_flags: number; // u32, @0
  expected_crc_valid: number; // u32, @4
  expected_framebuffer_crc: number; // u32, @8
}

/** SetView 0x0010: 112-byte record (implemented) */
export interface ZhRecordSetView {
  hdr: ZhCmdHeader;
  view_id: number; // u8, @0
  viewport_id: number; // u8, @1
  flags: number; // u16, @2
  view_projection: ZhMat4fx; // @4
  pixel_error: number; // fx16 (Q16.16, int32), @68
  geometry_tokens: number; // u32, @72
  fragment_tokens: number; // u32, @76
  eye: number[]; // fx16 (Q16.16, int32), @80
}

/** SetPresentationContract 0x0020: 48-byte record (implemented) */
export interface ZhRecordSetPresentationContract {
  hdr: ZhCmdHeader;
  mode: number; // video_mode (u8), @0
  view_count: number; // u8, @1
  flags: number; // u16, @2
  geometry_tokens: number[]; // u32, @4
  fragment_tokens: number[]; // u32, @12
  shared_tokens: number; // u32, @20
}

/** TerrainField 0x0200: 112-byte record (implemented) */
export interface ZhRecordTerrainField {
  hdr: ZhCmdHeader;
  program: number; // handle32, @0
  footprint: ZhRectfx; // @4
  start_tick: number; // u32, @20
  duration_ticks: number; // u32, @24
  parameters: number[]; // u8, @28
}

/** SurfaceStamp 0x0210: 64-byte record (implemented) */
export interface ZhRecordSurfaceStamp {
  hdr: ZhCmdHeader;
  brush: number; // handle32, @0
  patch: number; // handle32, @4
  operation: number; // u8, @8
  tag: number; // u8, @9
  strength: number; // u16, @10
  transform: ZhTransform2fx; // @12
  radius: number; // fx16 (Q16.16, int32), @36
  ring_width: number; // fx16 (Q16.16, int32), @40
}

/** TerrainEpoch 0x0220: 32-byte record (reserved) */
export interface ZhRecordTerrainEpoch {
  hdr: ZhCmdHeader;
  epoch: number; // u32, @0
  op: number; // u8, @4
  flags: number; // u8, @5
  reserved: number; // u16, @6
  island_table_handle: number; // u32, @8
  source_id: number; // u32, @12
}

/** SubmitTerrainSet 0x0230: 48-byte record (reserved) */
export interface ZhRecordSubmitTerrainSet {
  hdr: ZhCmdHeader;
  resource_epoch: number; // u32, @0
  list_offset: number; // u32, @4
  list_bytes: number; // u32, @8
  list_crc32c: number; // u32, @12
  patch_count: number; // u16, @16
  view_mask: number; // u8, @18
  flags: number; // u8, @19
  sequence: number; // u32, @20
  reserved0: number; // u32, @24
  reserved1: number; // u32, @28
}

/** DrawForm 0x0300: 32-byte record (implemented) */
export interface ZhRecordDrawForm {
  hdr: ZhCmdHeader;
  form: number; // handle32, @0
  material_set: number; // handle32, @4
  transform: number; // handle32, @8
  viewport_mask: number; // u8, @12
  semantic_weight: number; // u8, @13
  flags: number; // u16, @14
}

/** DrawPopulation 0x0301: 32-byte record (implemented) */
export interface ZhRecordDrawPopulation {
  hdr: ZhCmdHeader;
  population: number; // handle32, @0
  viewport_mask: number; // u8, @4
  semantic_weight: number; // u8, @5
  flags: number; // u16, @6
}

/** DrawProcedural 0x0302: 64-byte record (implemented) */
export interface ZhRecordDrawProcedural {
  hdr: ZhCmdHeader;
  program: number; // handle32, @0
  material_set: number; // handle32, @4
  transform: ZhTransform2fx; // @8
  screen_error: number; // fx16 (Q16.16, int32), @32
  kind: number; // forge_kind (u8), @36
  frame_tick: number[]; // u8, @37
  material_id: number; // u16, @40
}

/** DrawSky 0x0310: 176-byte record (reserved) */
export interface ZhRecordDrawSky {
  hdr: ZhCmdHeader;
  sky_set: number; // handle32, @0
  rot_proj: ZhMat4fx[]; // @4
  cloud_scroll_u: number; // fx16 (Q16.16, int32), @132
  cloud_scroll_v: number; // fx16 (Q16.16, int32), @136
  drum_yaw: number; // angle16 (U 0.0.16 turns, u16), @140
  viewport_mask: number; // u8, @142
  flags: number; // u8, @143
  reserved0: number; // u8, @144
  reserved1: number; // u8, @145
}

/** SetEnvironment 0x0311: 48-byte record (implemented) */
export interface ZhRecordSetEnvironment {
  hdr: ZhCmdHeader;
  sun_yaw: number; // angle16 (U 0.0.16 turns, u16), @0
  sun_pitch: number; // angle16 (U 0.0.16 turns, u16), @2
  sun_colour: ZhRgb565; // @4
  ambient: ZhRgb565; // @6
  tint: ZhRgb565; // @8
  tint_strength: number; // u8, @10
  fog: number; // fog_mode (u8), @11
  fog_near: number; // fx16 (Q16.16, int32), @12
  fog_far: number; // fx16 (Q16.16, int32), @16
  terrain_material_set: number; // handle32, @20
  terrain_material_id: number; // u16, @24
}

/** EmitAudioEvent 0x0400: 32-byte record (implemented) */
export interface ZhRecordEmitAudioEvent {
  hdr: ZhCmdHeader;
  event_id: number; // u32, @0
  pan_fx: number; // i16, @4
  gain: number; // u16, @6
  sample_handle: number; // u32, @8
  timestamp: number; // u32, @12
}

/** DebugBootstrap 0xF001: 64-byte record (reserved) */
export interface ZhRecordDebugBootstrap {
  hdr: ZhCmdHeader;
  data: number[]; // u8, @0
}

/** DebugFrameBlit 0xF002: 48-byte record (implemented) */
export interface ZhRecordDebugFrameBlit {
  hdr: ZhCmdHeader;
  dst_slot: number; // u8, @0
  mode: number; // video_mode (u8), @1
  src_addr_hps: number; // u32, @4
  byte_len: number; // u32, @8
  expected_crc32c: number; // u32, @12
}

/** DebugRumble 0xF004: 32-byte record (implemented) */
export interface ZhRecordDebugRumble {
  hdr: ZhCmdHeader;
  pad_index: number; // u8, @0
  enable: number; // u8, @1
  strength: number; // u8, @2
}

/** PublishResource 0x0030: 48-byte record (implemented) */
export interface ZhRecordPublishResource {
  hdr: ZhCmdHeader;
  resource: number; // handle32, @0
  hps_addr_lo: number; // u32, @4
  hps_addr_hi: number; // u32, @8
  vram_dst: number; // u32, @12
  length: number; // u32, @16
  crc32c: number; // u32, @20
  new_generation: number; // u16, @24
  epoch: number; // u16, @26
  dst_slot: number; // u8, @28
  kind: number; // u8, @29
}

/** SetPost 0x0040: 32-byte record (implemented) */
export interface ZhRecordSetPost {
  hdr: ZhCmdHeader;
  bloom_gain: number; // u8, @0
  flags: number; // u8, @1
  flash_amount: number; // u8, @2
  bias_r: number; // i16, @4
  bias_g: number; // i16, @6
  bias_b: number; // i16, @8
  flash: ZhRgb565; // @10
  ink: ZhRgb565; // @12
}

/** SetGradeTable 0x0041: 96-byte record (implemented) */
export interface ZhRecordSetGradeTable {
  hdr: ZhCmdHeader;
  curve: number; // u8, @0
  first: number; // u8, @1
  count: number; // u8, @2
  vectors: number[]; // u8, @4
}

/** SetPopulation 0x0303: 48-byte record (implemented) */
export interface ZhRecordSetPopulation {
  hdr: ZhCmdHeader;
  population: number; // handle32, @0
  origin_x: number; // i32, @4
  origin_y: number; // i32, @8
  origin_z: number; // i32, @12
  active_count: number; // u32, @16
  plane_c: number; // i32, @20
  plane_nx: number; // i16, @24
  plane_ny: number; // i16, @26
  plane_nz: number; // i16, @28
  flags: number; // u16, @30
}

/** DebugTraceArm 0xF003: 32-byte record (implemented) */
export interface ZhRecordDebugTraceArm {
  hdr: ZhCmdHeader;
  stage_mask: number; // u8, @0
  flags: number; // u8, @1
}

/** DrawPosedForm 0x0305: 48-byte record (implemented) */
export interface ZhRecordDrawPosedForm {
  hdr: ZhCmdHeader;
  form: number; // handle32, @0
  material_set: number; // handle32, @4
  transform: number; // handle32, @8
  viewport_mask: number; // u8, @12
  semantic_weight: number; // u8, @13
  flags: number; // u16, @14
  clip_id: number; // u16, @16
  frame_no: number; // u16, @18
  sub: number; // u8, @20
}

/** DrawWarpedForm 0x0304: 96-byte record (implemented) */
export interface ZhRecordDrawWarpedForm {
  hdr: ZhCmdHeader;
  form: number; // handle32, @0
  material_set: number; // handle32, @4
  transform: number; // handle32, @8
  viewport_mask: number; // u8, @12
  semantic_weight: number; // u8, @13
  flags: number; // u16, @14
  warp_program: number; // handle32, @16
  time: number; // u32, @20
  params: number[]; // fx16 (Q16.16, int32), @24
  attributes: number[]; // fx16 (Q16.16, int32), @40
  warp_attributes: number; // handle32, @56
  attribute_mode: number; // warp_attribute_mode (u8), @60
  warp_flags: number; // u8, @61
  displacement_bound: number[]; // fx16 (Q16.16, int32), @64
}

/** SetPlane 0x0306: 64-byte record (implemented) */
export interface ZhRecordSetPlane {
  hdr: ZhCmdHeader;
  slot: number; // u8, @0
  role: number; // u8, @1
  blend: number; // u8, @2
  opacity: number; // u8, @3
  format: number; // u8, @4
  wrap: number; // u8, @5
  view_mask: number; // u8, @6
  palette_id: number; // u8, @7
  width: number; // u16, @8
  height: number; // u16, @10
  flags: number; // u16, @12
  base: number; // u16, @14
  lstride: number; // u8, @16
  lheight: number; // u8, @17
  a: number; // fx16 (Q16.16, int32), @20
  b: number; // fx16 (Q16.16, int32), @24
  c: number; // fx16 (Q16.16, int32), @28
  d: number; // fx16 (Q16.16, int32), @32
  u0: number; // fx16 (Q16.16, int32), @36
  v0: number; // fx16 (Q16.16, int32), @40
  line_scroll: number; // fx16 (Q16.16, int32), @44
}

/** DrawSprite 0x0307: 64-byte record (implemented) */
export interface ZhRecordDrawSprite {
  hdr: ZhCmdHeader;
  x: number; // i16, @0
  y: number; // i16, @2
  w: number; // u16, @4
  h: number; // u16, @6
  base: number; // u16, @8
  lstride: number; // u8, @10
  lheight: number; // u8, @11
  format: number; // u8, @12
  palette_id: number; // u8, @13
  blend: number; // u8, @14
  view_mask: number; // u8, @15
  tint: ZhRgb565; // @16
  order: number; // u8, @18
  flags: number; // u8, @19
  src_id: number; // u16, @20
  u: number; // fx16 (Q16.16, int32), @24
  v: number; // fx16 (Q16.16, int32), @28
  a00: number; // fx16 (Q16.16, int32), @32
  a01: number; // fx16 (Q16.16, int32), @36
  a10: number; // fx16 (Q16.16, int32), @40
  a11: number; // fx16 (Q16.16, int32), @44
}

export interface ZhCommandInfo {
  name: string;
  opcode: number;
  recordBytes: number;
  implemented: boolean;
  /** payload-relative byte offsets that must be zero (pads) */
  padOffsets: readonly number[];
  /** enum range checks (capture_format.md 3.2 step 7, ABI v2) */
  enumChecks: readonly { readonly offset: number; readonly size: number; readonly values: readonly number[] }[];
}
export const ZHAO_COMMAND_TABLE: readonly ZhCommandInfo[] = [
  { name: 'Nop', opcode: 0x0000, recordBytes: 16, implemented: true, padOffsets: [], enumChecks: [] },
  { name: 'BeginFrame', opcode: 0x0001, recordBytes: 32, implemented: true, padOffsets: [], enumChecks: [] },
  { name: 'SealFramePlan', opcode: 0x0003, recordBytes: 48, implemented: true, padOffsets: [28, 29, 30, 31], enumChecks: [] },
  { name: 'EndFrame', opcode: 0x0002, recordBytes: 32, implemented: true, padOffsets: [12, 13, 14, 15], enumChecks: [] },
  { name: 'SetView', opcode: 0x0010, recordBytes: 112, implemented: true, padOffsets: [92, 93, 94, 95], enumChecks: [] },
  { name: 'SetPresentationContract', opcode: 0x0020, recordBytes: 48, implemented: true, padOffsets: [24, 25, 26, 27, 28, 29, 30, 31], enumChecks: [{ offset: 0, size: 1, values: [0, 1, 2] }] },
  { name: 'TerrainField', opcode: 0x0200, recordBytes: 112, implemented: true, padOffsets: [92, 93, 94, 95], enumChecks: [] },
  { name: 'SurfaceStamp', opcode: 0x0210, recordBytes: 64, implemented: true, padOffsets: [44, 45, 46, 47], enumChecks: [] },
  { name: 'TerrainEpoch', opcode: 0x0220, recordBytes: 32, implemented: false, padOffsets: [], enumChecks: [] },
  { name: 'SubmitTerrainSet', opcode: 0x0230, recordBytes: 48, implemented: false, padOffsets: [], enumChecks: [] },
  { name: 'DrawForm', opcode: 0x0300, recordBytes: 32, implemented: true, padOffsets: [], enumChecks: [] },
  { name: 'DrawPopulation', opcode: 0x0301, recordBytes: 32, implemented: true, padOffsets: [8, 9, 10, 11, 12, 13, 14, 15], enumChecks: [] },
  { name: 'DrawProcedural', opcode: 0x0302, recordBytes: 64, implemented: true, padOffsets: [39, 42, 43, 44, 45, 46, 47], enumChecks: [{ offset: 36, size: 1, values: [0, 1, 2, 3, 4, 5] }] },
  { name: 'DrawSky', opcode: 0x0310, recordBytes: 176, implemented: false, padOffsets: [146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159], enumChecks: [] },
  { name: 'SetEnvironment', opcode: 0x0311, recordBytes: 48, implemented: true, padOffsets: [26, 27, 28, 29, 30, 31], enumChecks: [{ offset: 11, size: 1, values: [0, 1] }] },
  { name: 'EmitAudioEvent', opcode: 0x0400, recordBytes: 32, implemented: true, padOffsets: [], enumChecks: [] },
  { name: 'DebugBootstrap', opcode: 0xF001, recordBytes: 64, implemented: false, padOffsets: [], enumChecks: [] },
  { name: 'DebugFrameBlit', opcode: 0xF002, recordBytes: 48, implemented: true, padOffsets: [2, 3, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31], enumChecks: [{ offset: 1, size: 1, values: [0, 1, 2] }] },
  { name: 'DebugRumble', opcode: 0xF004, recordBytes: 32, implemented: true, padOffsets: [3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15], enumChecks: [] },
  { name: 'PublishResource', opcode: 0x0030, recordBytes: 48, implemented: true, padOffsets: [30, 31], enumChecks: [] },
  { name: 'SetPost', opcode: 0x0040, recordBytes: 32, implemented: true, padOffsets: [3, 14, 15], enumChecks: [] },
  { name: 'SetGradeTable', opcode: 0x0041, recordBytes: 96, implemented: true, padOffsets: [3, 76, 77, 78, 79], enumChecks: [] },
  { name: 'SetPopulation', opcode: 0x0303, recordBytes: 48, implemented: true, padOffsets: [], enumChecks: [] },
  { name: 'DebugTraceArm', opcode: 0xF003, recordBytes: 32, implemented: true, padOffsets: [2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15], enumChecks: [] },
  { name: 'DrawPosedForm', opcode: 0x0305, recordBytes: 48, implemented: true, padOffsets: [21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31], enumChecks: [] },
  { name: 'DrawWarpedForm', opcode: 0x0304, recordBytes: 96, implemented: true, padOffsets: [62, 63, 76, 77, 78, 79], enumChecks: [{ offset: 60, size: 1, values: [0, 1] }] },
  { name: 'SetPlane', opcode: 0x0306, recordBytes: 64, implemented: true, padOffsets: [18, 19], enumChecks: [] },
  { name: 'DrawSprite', opcode: 0x0307, recordBytes: 64, implemented: true, padOffsets: [22, 23], enumChecks: [] },
];
export const ZHAO_COMMAND_COUNT = 28 as const;
export const ZHAO_MAX_RECORD_BYTES = 176 as const;
export function zhaoCommandInfo(opcode: number): ZhCommandInfo | undefined {
  return ZHAO_COMMAND_TABLE.find((c) => c.opcode === opcode);
}

// CRC-32C (Castagnoli): poly 0x82F63B78 reflected, init/xorout 0xFFFFFFFF
export const ZHAO_CRC32C_TABLE: readonly number[] = [
  0x00000000, 0xf26b8303, 0xe13b70f7, 0x1350f3f4, 0xc79a971f, 0x35f1141c, 0x26a1e7e8, 0xd4ca64eb,
  0x8ad958cf, 0x78b2dbcc, 0x6be22838, 0x9989ab3b, 0x4d43cfd0, 0xbf284cd3, 0xac78bf27, 0x5e133c24,
  0x105ec76f, 0xe235446c, 0xf165b798, 0x030e349b, 0xd7c45070, 0x25afd373, 0x36ff2087, 0xc494a384,
  0x9a879fa0, 0x68ec1ca3, 0x7bbcef57, 0x89d76c54, 0x5d1d08bf, 0xaf768bbc, 0xbc267848, 0x4e4dfb4b,
  0x20bd8ede, 0xd2d60ddd, 0xc186fe29, 0x33ed7d2a, 0xe72719c1, 0x154c9ac2, 0x061c6936, 0xf477ea35,
  0xaa64d611, 0x580f5512, 0x4b5fa6e6, 0xb93425e5, 0x6dfe410e, 0x9f95c20d, 0x8cc531f9, 0x7eaeb2fa,
  0x30e349b1, 0xc288cab2, 0xd1d83946, 0x23b3ba45, 0xf779deae, 0x05125dad, 0x1642ae59, 0xe4292d5a,
  0xba3a117e, 0x4851927d, 0x5b016189, 0xa96ae28a, 0x7da08661, 0x8fcb0562, 0x9c9bf696, 0x6ef07595,
  0x417b1dbc, 0xb3109ebf, 0xa0406d4b, 0x522bee48, 0x86e18aa3, 0x748a09a0, 0x67dafa54, 0x95b17957,
  0xcba24573, 0x39c9c670, 0x2a993584, 0xd8f2b687, 0x0c38d26c, 0xfe53516f, 0xed03a29b, 0x1f682198,
  0x5125dad3, 0xa34e59d0, 0xb01eaa24, 0x42752927, 0x96bf4dcc, 0x64d4cecf, 0x77843d3b, 0x85efbe38,
  0xdbfc821c, 0x2997011f, 0x3ac7f2eb, 0xc8ac71e8, 0x1c661503, 0xee0d9600, 0xfd5d65f4, 0x0f36e6f7,
  0x61c69362, 0x93ad1061, 0x80fde395, 0x72966096, 0xa65c047d, 0x5437877e, 0x4767748a, 0xb50cf789,
  0xeb1fcbad, 0x197448ae, 0x0a24bb5a, 0xf84f3859, 0x2c855cb2, 0xdeeedfb1, 0xcdbe2c45, 0x3fd5af46,
  0x7198540d, 0x83f3d70e, 0x90a324fa, 0x62c8a7f9, 0xb602c312, 0x44694011, 0x5739b3e5, 0xa55230e6,
  0xfb410cc2, 0x092a8fc1, 0x1a7a7c35, 0xe811ff36, 0x3cdb9bdd, 0xceb018de, 0xdde0eb2a, 0x2f8b6829,
  0x82f63b78, 0x709db87b, 0x63cd4b8f, 0x91a6c88c, 0x456cac67, 0xb7072f64, 0xa457dc90, 0x563c5f93,
  0x082f63b7, 0xfa44e0b4, 0xe9141340, 0x1b7f9043, 0xcfb5f4a8, 0x3dde77ab, 0x2e8e845f, 0xdce5075c,
  0x92a8fc17, 0x60c37f14, 0x73938ce0, 0x81f80fe3, 0x55326b08, 0xa759e80b, 0xb4091bff, 0x466298fc,
  0x1871a4d8, 0xea1a27db, 0xf94ad42f, 0x0b21572c, 0xdfeb33c7, 0x2d80b0c4, 0x3ed04330, 0xccbbc033,
  0xa24bb5a6, 0x502036a5, 0x4370c551, 0xb11b4652, 0x65d122b9, 0x97baa1ba, 0x84ea524e, 0x7681d14d,
  0x2892ed69, 0xdaf96e6a, 0xc9a99d9e, 0x3bc21e9d, 0xef087a76, 0x1d63f975, 0x0e330a81, 0xfc588982,
  0xb21572c9, 0x407ef1ca, 0x532e023e, 0xa145813d, 0x758fe5d6, 0x87e466d5, 0x94b49521, 0x66df1622,
  0x38cc2a06, 0xcaa7a905, 0xd9f75af1, 0x2b9cd9f2, 0xff56bd19, 0x0d3d3e1a, 0x1e6dcdee, 0xec064eed,
  0xc38d26c4, 0x31e6a5c7, 0x22b65633, 0xd0ddd530, 0x0417b1db, 0xf67c32d8, 0xe52cc12c, 0x1747422f,
  0x49547e0b, 0xbb3ffd08, 0xa86f0efc, 0x5a048dff, 0x8ecee914, 0x7ca56a17, 0x6ff599e3, 0x9d9e1ae0,
  0xd3d3e1ab, 0x21b862a8, 0x32e8915c, 0xc083125f, 0x144976b4, 0xe622f5b7, 0xf5720643, 0x07198540,
  0x590ab964, 0xab613a67, 0xb831c993, 0x4a5a4a90, 0x9e902e7b, 0x6cfbad78, 0x7fab5e8c, 0x8dc0dd8f,
  0xe330a81a, 0x115b2b19, 0x020bd8ed, 0xf0605bee, 0x24aa3f05, 0xd6c1bc06, 0xc5914ff2, 0x37faccf1,
  0x69e9f0d5, 0x9b8273d6, 0x88d28022, 0x7ab90321, 0xae7367ca, 0x5c18e4c9, 0x4f48173d, 0xbd23943e,
  0xf36e6f75, 0x0105ec76, 0x12551f82, 0xe03e9c81, 0x34f4f86a, 0xc69f7b69, 0xd5cf889d, 0x27a40b9e,
  0x79b737ba, 0x8bdcb4b9, 0x988c474d, 0x6ae7c44e, 0xbe2da0a5, 0x4c4623a6, 0x5f16d052, 0xad7d5351,
];
/** running form: crc=0 fresh, feed the previous return to continue (capture_format.md 2) */
export function crc32c(crc: number, buf: Uint8Array, off = 0, len = buf.length - off): number {
  let c = (crc ^ 0xffffffff) >>> 0;
  for (let i = 0; i < len; i++) {
    c = ((ZHAO_CRC32C_TABLE[(c ^ buf[off + i]!) & 0xff]! ^ (c >>> 8)) & 0xffffffff) >>> 0;
  }
  return (c ^ 0xffffffff) >>> 0;
}

const SHA256_K = new Uint32Array([
  0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
  0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
  0xe49b69c1, 0xefbe4786, 0xfc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
  0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x6ca6351, 0x14292967,
  0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
  0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
  0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
  0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
]);
function rotr(x: number, n: number): number {
  return ((x >>> n) | (x << (32 - n))) >>> 0;
}
/** SHA-256 (FIPS 180-4). Byte-identical to zref_sha256.hpp (locked by goldens). */
export function sha256(data: Uint8Array): Uint8Array {
  const h = new Uint32Array([0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19]);
  const bitLen = data.length * 8;
  const padded = new Uint8Array((((data.length + 8) >> 6) + 1) << 6);
  padded.set(data);
  padded[data.length] = 0x80;
  const dv = new DataView(padded.buffer);
  dv.setUint32(padded.length - 4, bitLen >>> 0, false);
  dv.setUint32(padded.length - 8, Math.floor(bitLen / 0x100000000), false);
  const w = new Uint32Array(64);
  for (let chunk = 0; chunk < padded.length; chunk += 64) {
    for (let i = 0; i < 16; i++) w[i] = dv.getUint32(chunk + i * 4, false);
    for (let i = 16; i < 64; i++) {
      const s0 = rotr(w[i - 15]!, 7) ^ rotr(w[i - 15]!, 18) ^ (w[i - 15]! >>> 3);
      const s1 = rotr(w[i - 2]!, 17) ^ rotr(w[i - 2]!, 19) ^ (w[i - 2]! >>> 10);
      w[i] = (w[i - 16]! + s0 + w[i - 7]! + s1) >>> 0;
    }
    let [a, b, c, d, e, f, g, hh] = [h[0]!, h[1]!, h[2]!, h[3]!, h[4]!, h[5]!, h[6]!, h[7]!];
    for (let i = 0; i < 64; i++) {
      const S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
      const ch = (e & f) ^ (~e & g);
      const t1 = (hh + S1 + ch + SHA256_K[i]! + w[i]!) >>> 0;
      const S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
      const maj = (a & b) ^ (a & c) ^ (b & c);
      const t2 = (S0 + maj) >>> 0;
      hh = g; g = f; f = e; e = (d + t1) >>> 0; d = c; c = b; b = a; a = (t1 + t2) >>> 0;
    }
    h[0] = (h[0]! + a) >>> 0; h[1] = (h[1]! + b) >>> 0; h[2] = (h[2]! + c) >>> 0; h[3] = (h[3]! + d) >>> 0;
    h[4] = (h[4]! + e) >>> 0; h[5] = (h[5]! + f) >>> 0; h[6] = (h[6]! + g) >>> 0; h[7] = (h[7]! + hh) >>> 0;
  }
  const out = new Uint8Array(32);
  const odv = new DataView(out.buffer);
  for (let i = 0; i < 8; i++) odv.setUint32(i * 4, h[i]!, false);
  return out;
}

/** little-endian byte writer */
export class ZhByteWriter {
  readonly bytes: number[] = [];
  u8(v: number): void { this.bytes.push(v & 0xff); }
  u16(v: number): void { this.u8(v); this.u8(v >>> 8); }
  u32(v: number): void { this.u16(v & 0xffff); this.u16((v >>> 16) & 0xffff); }
  i8(v: number): void { this.u8(v); }
  i16(v: number): void { this.u16(v & 0xffff); }
  i32(v: number): void { this.u32(v >>> 0); }
  fx16(v: number): void { this.i32(v); } // Q16.16
  zeros(n: number): void { for (let i = 0; i < n; i++) this.bytes.push(0); }
  raw(bytes: Uint8Array | readonly number[]): void { for (const b of bytes) this.bytes.push(b & 0xff); }
  toUint8Array(): Uint8Array { return Uint8Array.from(this.bytes); }
}

export function zhaoSampleMat4fx(): ZhMat4fx {
  return {
    m00: 88599,
    m01: 154135,
    m02: 219671,
    m03: 285207,
    m10: 350743,
    m11: 416279,
    m12: 481815,
    m13: 547351,
    m20: 88599,
    m21: 154135,
    m22: 219671,
    m23: 285207,
    m30: 350743,
    m31: 416279,
    m32: 481815,
    m33: 547351,
  };
}

export function zhaoSampleRectfx(): ZhRectfx {
  return {
    x0: 88599,
    y0: 154135,
    x1: 219671,
    y1: 285207,
  };
}

export function zhaoSampleTransform2fx(): ZhTransform2fx {
  return {
    tx: 88599,
    ty: 154135,
    r00: 219671,
    r01: 285207,
    r10: 350743,
    r11: 416279,
  };
}

export function zhaoSampleRgb565(): ZhRgb565 {
  return {
    bits: 60833,
  };
}

export function zhaoSampleNop(): ZhRecordNop {
  return {
    hdr: {
      opcode: ZHAO_OP_NOP,
      recordBytes: 16,
      sourceId: 1342242816, // kind 5, module 1, index 0
      flags: 0,
    },
  };
}

export function zhaoSampleBeginFrame(): ZhRecordBeginFrame {
  return {
    hdr: {
      opcode: ZHAO_OP_BEGIN_FRAME,
      recordBytes: 32,
      sourceId: 1342242817, // kind 5, module 1, index 1
      flags: 0,
    },
    frame_id: 0,
    resource_epoch: 0,
    flags: 0,
    deadline_cycles: 0,
  };
}

export function zhaoSampleSealFramePlan(): ZhRecordSealFramePlan {
  return {
    hdr: {
      opcode: ZHAO_OP_SEAL_FRAME_PLAN,
      recordBytes: 48,
      sourceId: 1342242818, // kind 5, module 1, index 2
      flags: 0,
    },
    view_id: 32,
    flags: 137,
    resource_gen: 63077,
    view_gen: 2654,
    giant_instance: 48408,
    plan_verts: 0,
    plan_tris: 0,
    plan_chunks: 0,
    plan_refs: 0,
    giant_refs: 0,
  };
}

export function zhaoSampleEndFrame(): ZhRecordEndFrame {
  return {
    hdr: {
      opcode: ZHAO_OP_END_FRAME,
      recordBytes: 32,
      sourceId: 1342242819, // kind 5, module 1, index 3
      flags: 0,
    },
    completion_flags: 0,
    expected_crc_valid: 0,
    expected_framebuffer_crc: 0,
  };
}

export function zhaoSampleSetView(): ZhRecordSetView {
  return {
    hdr: {
      opcode: ZHAO_OP_SET_VIEW,
      recordBytes: 112,
      sourceId: 1342242820, // kind 5, module 1, index 4
      flags: 0,
    },
    view_id: 32,
    viewport_id: 161,
    flags: 33377,
    view_projection: zhaoSampleMat4fx(),
    pixel_error: 285207,
    geometry_tokens: 0,
    fragment_tokens: 0,
    eye: [481815, 547351, 88599],
  };
}

export function zhaoSampleSetPresentationContract(): ZhRecordSetPresentationContract {
  return {
    hdr: {
      opcode: ZHAO_OP_SET_PRESENTATION_CONTRACT,
      recordBytes: 48,
      sourceId: 1342242821, // kind 5, module 1, index 5
      flags: 0,
    },
    mode: 0,
    view_count: 133,
    flags: 33377,
    geometry_tokens: [0, 0],
    fragment_tokens: [0, 0],
    shared_tokens: 0,
  };
}

export function zhaoSampleTerrainField(): ZhRecordTerrainField {
  return {
    hdr: {
      opcode: ZHAO_OP_TERRAIN_FIELD,
      recordBytes: 112,
      sourceId: 1342242822, // kind 5, module 1, index 6
      flags: 0,
    },
    program: 704643073,
    footprint: zhaoSampleRectfx(),
    start_tick: 0,
    duration_ticks: 0,
    parameters: [31, 21, 255, 30, 155, 81, 159, 149, 160, 162, 127, 136, 173, 166, 179, 252, 209, 106, 199, 112, 52, 18, 240, 38, 152, 74, 148, 250, 92, 122, 212, 50, 80, 190, 208, 254, 108, 138, 60, 58, 164, 26, 216, 78, 232, 2, 44, 242, 44, 66, 44, 234, 160, 178, 216, 34, 156, 174, 52, 50, 204, 42, 232, 190],
  };
}

export function zhaoSampleSurfaceStamp(): ZhRecordSurfaceStamp {
  return {
    hdr: {
      opcode: ZHAO_OP_SURFACE_STAMP,
      recordBytes: 64,
      sourceId: 1342242823, // kind 5, module 1, index 7
      flags: 0,
    },
    brush: 704643073,
    patch: 704643074,
    operation: 221,
    tag: 3,
    strength: 60507,
    transform: zhaoSampleTransform2fx(),
    radius: 285207,
    ring_width: 350743,
  };
}

export function zhaoSampleTerrainEpoch(): ZhRecordTerrainEpoch {
  return {
    hdr: {
      opcode: ZHAO_OP_TERRAIN_EPOCH,
      recordBytes: 32,
      sourceId: 1342242824, // kind 5, module 1, index 8
      flags: 0,
    },
    epoch: 0,
    op: 206,
    flags: 220,
    reserved: 33122,
    island_table_handle: 0,
    source_id: 0,
  };
}

export function zhaoSampleSubmitTerrainSet(): ZhRecordSubmitTerrainSet {
  return {
    hdr: {
      opcode: ZHAO_OP_SUBMIT_TERRAIN_SET,
      recordBytes: 48,
      sourceId: 1342242825, // kind 5, module 1, index 9
      flags: 0,
    },
    resource_epoch: 0,
    list_offset: 0,
    list_bytes: 0,
    list_crc32c: 0,
    patch_count: 60985,
    view_mask: 24,
    flags: 158,
    sequence: 0,
    reserved0: 0,
    reserved1: 0,
  };
}

export function zhaoSampleDrawForm(): ZhRecordDrawForm {
  return {
    hdr: {
      opcode: ZHAO_OP_DRAW_FORM,
      recordBytes: 32,
      sourceId: 1342242826, // kind 5, module 1, index 10
      flags: 0,
    },
    form: 704643073,
    material_set: 704643074,
    transform: 704643075,
    viewport_mask: 16,
    semantic_weight: 94,
    flags: 45880,
  };
}

export function zhaoSampleDrawPopulation(): ZhRecordDrawPopulation {
  return {
    hdr: {
      opcode: ZHAO_OP_DRAW_POPULATION,
      recordBytes: 32,
      sourceId: 1342242827, // kind 5, module 1, index 11
      flags: 0,
    },
    population: 704643073,
    viewport_mask: 254,
    semantic_weight: 96,
    flags: 9106,
  };
}

export function zhaoSampleDrawProcedural(): ZhRecordDrawProcedural {
  return {
    hdr: {
      opcode: ZHAO_OP_DRAW_PROCEDURAL,
      recordBytes: 64,
      sourceId: 1342242828, // kind 5, module 1, index 12
      flags: 0,
    },
    program: 704643073,
    material_set: 704643074,
    transform: zhaoSampleTransform2fx(),
    screen_error: 88599,
    kind: 3,
    frame_tick: [215, 157],
    material_id: 62550,
  };
}

export function zhaoSampleDrawSky(): ZhRecordDrawSky {
  return {
    hdr: {
      opcode: ZHAO_OP_DRAW_SKY,
      recordBytes: 176,
      sourceId: 1342242829, // kind 5, module 1, index 13
      flags: 0,
    },
    sky_set: 704643073,
    rot_proj: [zhaoSampleMat4fx(), zhaoSampleMat4fx()],
    cloud_scroll_u: 154135,
    cloud_scroll_v: 219671,
    drum_yaw: 44900,
    viewport_mask: 215,
    flags: 163,
    reserved0: 211,
    reserved1: 167,
  };
}

export function zhaoSampleSetEnvironment(): ZhRecordSetEnvironment {
  return {
    hdr: {
      opcode: ZHAO_OP_SET_ENVIRONMENT,
      recordBytes: 48,
      sourceId: 1342242830, // kind 5, module 1, index 14
      flags: 0,
    },
    sun_yaw: 22925,
    sun_pitch: 47428,
    sun_colour: zhaoSampleRgb565(),
    ambient: zhaoSampleRgb565(),
    tint: zhaoSampleRgb565(),
    tint_strength: 140,
    fog: 0,
    fog_near: 547351,
    fog_far: 88599,
    terrain_material_set: 704643082,
    terrain_material_id: 10022,
  };
}

export function zhaoSampleEmitAudioEvent(): ZhRecordEmitAudioEvent {
  return {
    hdr: {
      opcode: ZHAO_OP_EMIT_AUDIO_EVENT,
      recordBytes: 32,
      sourceId: 1342242831, // kind 5, module 1, index 15
      flags: 0,
    },
    event_id: 0,
    pan_fx: 19374,
    gain: 31691,
    sample_handle: 0,
    timestamp: 0,
  };
}

export function zhaoSampleDebugBootstrap(): ZhRecordDebugBootstrap {
  return {
    hdr: {
      opcode: ZHAO_OP_DEBUG_BOOTSTRAP,
      recordBytes: 64,
      sourceId: 1342242832, // kind 5, module 1, index 16
      flags: 0,
    },
    data: [113, 17, 53, 185, 245, 21, 109, 189, 137, 201, 49, 1, 201, 193, 109, 93, 73, 157, 233, 89, 81, 41, 61, 217, 149, 93, 245, 157, 153, 81, 117, 165, 129, 213, 185, 169, 113, 233, 253, 237, 21, 157, 21, 189, 17, 25, 93, 57],
  };
}

export function zhaoSampleDebugFrameBlit(): ZhRecordDebugFrameBlit {
  return {
    hdr: {
      opcode: ZHAO_OP_DEBUG_FRAME_BLIT,
      recordBytes: 48,
      sourceId: 1342242833, // kind 5, module 1, index 17
      flags: 0,
    },
    dst_slot: 53,
    mode: 1,
    src_addr_hps: 0,
    byte_len: 0,
    expected_crc32c: 0,
  };
}

export function zhaoSampleDebugRumble(): ZhRecordDebugRumble {
  return {
    hdr: {
      opcode: ZHAO_OP_DEBUG_RUMBLE,
      recordBytes: 32,
      sourceId: 1342242834, // kind 5, module 1, index 18
      flags: 0,
    },
    pad_index: 113,
    enable: 169,
    strength: 117,
  };
}

export function zhaoSamplePublishResource(): ZhRecordPublishResource {
  return {
    hdr: {
      opcode: ZHAO_OP_PUBLISH_RESOURCE,
      recordBytes: 48,
      sourceId: 1342242835, // kind 5, module 1, index 19
      flags: 0,
    },
    resource: 704643073,
    hps_addr_lo: 0,
    hps_addr_hi: 0,
    vram_dst: 0,
    length: 0,
    crc32c: 0,
    new_generation: 20931,
    epoch: 64290,
    dst_slot: 185,
    kind: 85,
  };
}

export function zhaoSampleSetPost(): ZhRecordSetPost {
  return {
    hdr: {
      opcode: ZHAO_OP_SET_POST,
      recordBytes: 32,
      sourceId: 1342242836, // kind 5, module 1, index 20
      flags: 0,
    },
    bloom_gain: 134,
    flags: 137,
    flash_amount: 145,
    bias_r: 30028,
    bias_g: 10970,
    bias_b: 22138,
    flash: zhaoSampleRgb565(),
    ink: zhaoSampleRgb565(),
  };
}

export function zhaoSampleSetGradeTable(): ZhRecordSetGradeTable {
  return {
    hdr: {
      opcode: ZHAO_OP_SET_GRADE_TABLE,
      recordBytes: 96,
      sourceId: 1342242837, // kind 5, module 1, index 21
      flags: 0,
    },
    curve: 205,
    first: 53,
    count: 109,
    vectors: [65, 153, 137, 145, 129, 85, 5, 173, 201, 193, 185, 65, 173, 41, 117, 13, 9, 85, 129, 73, 122, 122, 232, 226, 118, 6, 164, 158, 18, 178, 85, 13, 201, 181, 89, 65, 253, 89, 93, 181, 73, 193, 69, 145, 141, 21, 137, 149, 225, 153, 114, 90, 72, 162, 246, 30, 76, 166, 165, 181, 57, 153, 61, 129, 77, 221, 129, 117, 193, 193, 201, 233],
  };
}

export function zhaoSampleSetPopulation(): ZhRecordSetPopulation {
  return {
    hdr: {
      opcode: ZHAO_OP_SET_POPULATION,
      recordBytes: 48,
      sourceId: 1342242838, // kind 5, module 1, index 22
      flags: 0,
    },
    population: 704643073,
    origin_x: 0,
    origin_y: 0,
    origin_z: 0,
    active_count: 0,
    plane_c: 0,
    plane_nx: 4151,
    plane_ny: 59330,
    plane_nz: 55757,
    flags: 644,
  };
}

export function zhaoSampleDebugTraceArm(): ZhRecordDebugTraceArm {
  return {
    hdr: {
      opcode: ZHAO_OP_DEBUG_TRACE_ARM,
      recordBytes: 32,
      sourceId: 1342242839, // kind 5, module 1, index 23
      flags: 0,
    },
    stage_mask: 165,
    flags: 137,
  };
}

export function zhaoSampleDrawPosedForm(): ZhRecordDrawPosedForm {
  return {
    hdr: {
      opcode: ZHAO_OP_DRAW_POSED_FORM,
      recordBytes: 48,
      sourceId: 1342242840, // kind 5, module 1, index 24
      flags: 0,
    },
    form: 704643073,
    material_set: 704643074,
    transform: 704643075,
    viewport_mask: 16,
    semantic_weight: 94,
    flags: 45880,
    clip_id: 59139,
    frame_no: 64773,
    sub: 205,
  };
}

export function zhaoSampleDrawWarpedForm(): ZhRecordDrawWarpedForm {
  return {
    hdr: {
      opcode: ZHAO_OP_DRAW_WARPED_FORM,
      recordBytes: 96,
      sourceId: 1342242841, // kind 5, module 1, index 25
      flags: 0,
    },
    form: 704643073,
    material_set: 704643074,
    transform: 704643075,
    viewport_mask: 16,
    semantic_weight: 94,
    flags: 45880,
    warp_program: 704643079,
    time: 0,
    params: [88599, 154135, 219671, 285207],
    attributes: [350743, 416279, 481815, 547351],
    warp_attributes: 704643089,
    attribute_mode: 1,
    warp_flags: 184,
    displacement_bound: [416279, 481815, 547351],
  };
}

export function zhaoSampleSetPlane(): ZhRecordSetPlane {
  return {
    hdr: {
      opcode: ZHAO_OP_SET_PLANE,
      recordBytes: 64,
      sourceId: 1342242842, // kind 5, module 1, index 26
      flags: 0,
    },
    slot: 125,
    role: 253,
    blend: 33,
    opacity: 245,
    format: 141,
    wrap: 129,
    view_mask: 125,
    palette_id: 157,
    width: 9545,
    height: 22272,
    flags: 15507,
    base: 7226,
    lstride: 253,
    lheight: 217,
    a: 88599,
    b: 154135,
    c: 219671,
    d: 285207,
    u0: 350743,
    v0: 416279,
    line_scroll: 481815,
  };
}

export function zhaoSampleDrawSprite(): ZhRecordDrawSprite {
  return {
    hdr: {
      opcode: ZHAO_OP_DRAW_SPRITE,
      recordBytes: 64,
      sourceId: 1342242843, // kind 5, module 1, index 27
      flags: 0,
    },
    x: 10553,
    y: 10616,
    w: 31735,
    h: 28502,
    base: 35129,
    lstride: 240,
    lheight: 90,
    format: 56,
    palette_id: 102,
    blend: 180,
    view_mask: 2,
    tint: zhaoSampleRgb565(),
    order: 13,
    flags: 231,
    src_id: 25795,
    u: 154135,
    v: 219671,
    a00: 285207,
    a01: 350743,
    a10: 416279,
    a11: 481815,
  };
}

export function zhaoPackMat4fx(v: ZhMat4fx, w: ZhByteWriter): void {
  w.fx16(v.m00);
  w.fx16(v.m01);
  w.fx16(v.m02);
  w.fx16(v.m03);
  w.fx16(v.m10);
  w.fx16(v.m11);
  w.fx16(v.m12);
  w.fx16(v.m13);
  w.fx16(v.m20);
  w.fx16(v.m21);
  w.fx16(v.m22);
  w.fx16(v.m23);
  w.fx16(v.m30);
  w.fx16(v.m31);
  w.fx16(v.m32);
  w.fx16(v.m33);
}

export function zhaoPackRectfx(v: ZhRectfx, w: ZhByteWriter): void {
  w.fx16(v.x0);
  w.fx16(v.y0);
  w.fx16(v.x1);
  w.fx16(v.y1);
}

export function zhaoPackTransform2fx(v: ZhTransform2fx, w: ZhByteWriter): void {
  w.fx16(v.tx);
  w.fx16(v.ty);
  w.fx16(v.r00);
  w.fx16(v.r01);
  w.fx16(v.r10);
  w.fx16(v.r11);
}

export function zhaoPackRgb565(v: ZhRgb565, w: ZhByteWriter): void {
  w.u16(v.bits);
}

export function zhaoPackNop(r: ZhRecordNop, w: ZhByteWriter): void {
  w.u16(r.hdr.opcode); w.u16(r.hdr.recordBytes); w.u32(r.hdr.sourceId);
  w.u32(r.hdr.flags); w.zeros(4); // reserved0
}

export function zhaoPackBeginFrame(r: ZhRecordBeginFrame, w: ZhByteWriter): void {
  w.u16(r.hdr.opcode); w.u16(r.hdr.recordBytes); w.u32(r.hdr.sourceId);
  w.u32(r.hdr.flags); w.zeros(4); // reserved0
  w.u32(r.frame_id);
  w.u32(r.resource_epoch);
  w.u32(r.flags);
  w.u32(r.deadline_cycles);
}

export function zhaoPackSealFramePlan(r: ZhRecordSealFramePlan, w: ZhByteWriter): void {
  w.u16(r.hdr.opcode); w.u16(r.hdr.recordBytes); w.u32(r.hdr.sourceId);
  w.u32(r.hdr.flags); w.zeros(4); // reserved0
  w.u8(r.view_id);
  w.u8(r.flags);
  w.u16(r.resource_gen);
  w.u16(r.view_gen);
  w.u16(r.giant_instance);
  w.u32(r.plan_verts);
  w.u32(r.plan_tris);
  w.u32(r.plan_chunks);
  w.u32(r.plan_refs);
  w.u32(r.giant_refs);
  w.zeros(4); // pad
}

export function zhaoPackEndFrame(r: ZhRecordEndFrame, w: ZhByteWriter): void {
  w.u16(r.hdr.opcode); w.u16(r.hdr.recordBytes); w.u32(r.hdr.sourceId);
  w.u32(r.hdr.flags); w.zeros(4); // reserved0
  w.u32(r.completion_flags);
  w.u32(r.expected_crc_valid);
  w.u32(r.expected_framebuffer_crc);
  w.zeros(4); // pad
}

export function zhaoPackSetView(r: ZhRecordSetView, w: ZhByteWriter): void {
  w.u16(r.hdr.opcode); w.u16(r.hdr.recordBytes); w.u32(r.hdr.sourceId);
  w.u32(r.hdr.flags); w.zeros(4); // reserved0
  w.u8(r.view_id);
  w.u8(r.viewport_id);
  w.u16(r.flags);
  zhaoPackMat4fx(r.view_projection, w);
  w.fx16(r.pixel_error);
  w.u32(r.geometry_tokens);
  w.u32(r.fragment_tokens);
  for (let i = 0; i < 3; i++) w.fx16(r.eye[i]!);
  w.zeros(4); // pad
}

export function zhaoPackSetPresentationContract(r: ZhRecordSetPresentationContract, w: ZhByteWriter): void {
  w.u16(r.hdr.opcode); w.u16(r.hdr.recordBytes); w.u32(r.hdr.sourceId);
  w.u32(r.hdr.flags); w.zeros(4); // reserved0
  w.u8(r.mode);
  w.u8(r.view_count);
  w.u16(r.flags);
  for (let i = 0; i < 2; i++) w.u32(r.geometry_tokens[i]!);
  for (let i = 0; i < 2; i++) w.u32(r.fragment_tokens[i]!);
  w.u32(r.shared_tokens);
  w.zeros(8); // pad
}

export function zhaoPackTerrainField(r: ZhRecordTerrainField, w: ZhByteWriter): void {
  w.u16(r.hdr.opcode); w.u16(r.hdr.recordBytes); w.u32(r.hdr.sourceId);
  w.u32(r.hdr.flags); w.zeros(4); // reserved0
  w.u32(r.program);
  zhaoPackRectfx(r.footprint, w);
  w.u32(r.start_tick);
  w.u32(r.duration_ticks);
  for (let i = 0; i < 64; i++) w.u8(r.parameters[i]!);
  w.zeros(4); // pad
}

export function zhaoPackSurfaceStamp(r: ZhRecordSurfaceStamp, w: ZhByteWriter): void {
  w.u16(r.hdr.opcode); w.u16(r.hdr.recordBytes); w.u32(r.hdr.sourceId);
  w.u32(r.hdr.flags); w.zeros(4); // reserved0
  w.u32(r.brush);
  w.u32(r.patch);
  w.u8(r.operation);
  w.u8(r.tag);
  w.u16(r.strength);
  zhaoPackTransform2fx(r.transform, w);
  w.fx16(r.radius);
  w.fx16(r.ring_width);
  w.zeros(4); // pad
}

export function zhaoPackTerrainEpoch(r: ZhRecordTerrainEpoch, w: ZhByteWriter): void {
  w.u16(r.hdr.opcode); w.u16(r.hdr.recordBytes); w.u32(r.hdr.sourceId);
  w.u32(r.hdr.flags); w.zeros(4); // reserved0
  w.u32(r.epoch);
  w.u8(r.op);
  w.u8(r.flags);
  w.u16(r.reserved);
  w.u32(r.island_table_handle);
  w.u32(r.source_id);
}

export function zhaoPackSubmitTerrainSet(r: ZhRecordSubmitTerrainSet, w: ZhByteWriter): void {
  w.u16(r.hdr.opcode); w.u16(r.hdr.recordBytes); w.u32(r.hdr.sourceId);
  w.u32(r.hdr.flags); w.zeros(4); // reserved0
  w.u32(r.resource_epoch);
  w.u32(r.list_offset);
  w.u32(r.list_bytes);
  w.u32(r.list_crc32c);
  w.u16(r.patch_count);
  w.u8(r.view_mask);
  w.u8(r.flags);
  w.u32(r.sequence);
  w.u32(r.reserved0);
  w.u32(r.reserved1);
}

export function zhaoPackDrawForm(r: ZhRecordDrawForm, w: ZhByteWriter): void {
  w.u16(r.hdr.opcode); w.u16(r.hdr.recordBytes); w.u32(r.hdr.sourceId);
  w.u32(r.hdr.flags); w.zeros(4); // reserved0
  w.u32(r.form);
  w.u32(r.material_set);
  w.u32(r.transform);
  w.u8(r.viewport_mask);
  w.u8(r.semantic_weight);
  w.u16(r.flags);
}

export function zhaoPackDrawPopulation(r: ZhRecordDrawPopulation, w: ZhByteWriter): void {
  w.u16(r.hdr.opcode); w.u16(r.hdr.recordBytes); w.u32(r.hdr.sourceId);
  w.u32(r.hdr.flags); w.zeros(4); // reserved0
  w.u32(r.population);
  w.u8(r.viewport_mask);
  w.u8(r.semantic_weight);
  w.u16(r.flags);
  w.zeros(8); // pad
}

export function zhaoPackDrawProcedural(r: ZhRecordDrawProcedural, w: ZhByteWriter): void {
  w.u16(r.hdr.opcode); w.u16(r.hdr.recordBytes); w.u32(r.hdr.sourceId);
  w.u32(r.hdr.flags); w.zeros(4); // reserved0
  w.u32(r.program);
  w.u32(r.material_set);
  zhaoPackTransform2fx(r.transform, w);
  w.fx16(r.screen_error);
  w.u8(r.kind);
  for (let i = 0; i < 2; i++) w.u8(r.frame_tick[i]!);
  w.zeros(1); // pad
  w.u16(r.material_id);
  w.zeros(6); // pad_1
}

export function zhaoPackDrawSky(r: ZhRecordDrawSky, w: ZhByteWriter): void {
  w.u16(r.hdr.opcode); w.u16(r.hdr.recordBytes); w.u32(r.hdr.sourceId);
  w.u32(r.hdr.flags); w.zeros(4); // reserved0
  w.u32(r.sky_set);
  zhaoPackMat4fx(r.rot_proj[0]!, w);
  zhaoPackMat4fx(r.rot_proj[1]!, w);
  w.fx16(r.cloud_scroll_u);
  w.fx16(r.cloud_scroll_v);
  w.u16(r.drum_yaw);
  w.u8(r.viewport_mask);
  w.u8(r.flags);
  w.u8(r.reserved0);
  w.u8(r.reserved1);
  w.zeros(14); // pad
}

export function zhaoPackSetEnvironment(r: ZhRecordSetEnvironment, w: ZhByteWriter): void {
  w.u16(r.hdr.opcode); w.u16(r.hdr.recordBytes); w.u32(r.hdr.sourceId);
  w.u32(r.hdr.flags); w.zeros(4); // reserved0
  w.u16(r.sun_yaw);
  w.u16(r.sun_pitch);
  zhaoPackRgb565(r.sun_colour, w);
  zhaoPackRgb565(r.ambient, w);
  zhaoPackRgb565(r.tint, w);
  w.u8(r.tint_strength);
  w.u8(r.fog);
  w.fx16(r.fog_near);
  w.fx16(r.fog_far);
  w.u32(r.terrain_material_set);
  w.u16(r.terrain_material_id);
  w.zeros(6); // pad
}

export function zhaoPackEmitAudioEvent(r: ZhRecordEmitAudioEvent, w: ZhByteWriter): void {
  w.u16(r.hdr.opcode); w.u16(r.hdr.recordBytes); w.u32(r.hdr.sourceId);
  w.u32(r.hdr.flags); w.zeros(4); // reserved0
  w.u32(r.event_id);
  w.i16(r.pan_fx);
  w.u16(r.gain);
  w.u32(r.sample_handle);
  w.u32(r.timestamp);
}

export function zhaoPackDebugBootstrap(r: ZhRecordDebugBootstrap, w: ZhByteWriter): void {
  w.u16(r.hdr.opcode); w.u16(r.hdr.recordBytes); w.u32(r.hdr.sourceId);
  w.u32(r.hdr.flags); w.zeros(4); // reserved0
  for (let i = 0; i < 48; i++) w.u8(r.data[i]!);
}

export function zhaoPackDebugFrameBlit(r: ZhRecordDebugFrameBlit, w: ZhByteWriter): void {
  w.u16(r.hdr.opcode); w.u16(r.hdr.recordBytes); w.u32(r.hdr.sourceId);
  w.u32(r.hdr.flags); w.zeros(4); // reserved0
  w.u8(r.dst_slot);
  w.u8(r.mode);
  w.zeros(2); // pad
  w.u32(r.src_addr_hps);
  w.u32(r.byte_len);
  w.u32(r.expected_crc32c);
  w.zeros(16); // pad_1
}

export function zhaoPackDebugRumble(r: ZhRecordDebugRumble, w: ZhByteWriter): void {
  w.u16(r.hdr.opcode); w.u16(r.hdr.recordBytes); w.u32(r.hdr.sourceId);
  w.u32(r.hdr.flags); w.zeros(4); // reserved0
  w.u8(r.pad_index);
  w.u8(r.enable);
  w.u8(r.strength);
  w.zeros(13); // pad
}

export function zhaoPackPublishResource(r: ZhRecordPublishResource, w: ZhByteWriter): void {
  w.u16(r.hdr.opcode); w.u16(r.hdr.recordBytes); w.u32(r.hdr.sourceId);
  w.u32(r.hdr.flags); w.zeros(4); // reserved0
  w.u32(r.resource);
  w.u32(r.hps_addr_lo);
  w.u32(r.hps_addr_hi);
  w.u32(r.vram_dst);
  w.u32(r.length);
  w.u32(r.crc32c);
  w.u16(r.new_generation);
  w.u16(r.epoch);
  w.u8(r.dst_slot);
  w.u8(r.kind);
  w.zeros(2); // pad
}

export function zhaoPackSetPost(r: ZhRecordSetPost, w: ZhByteWriter): void {
  w.u16(r.hdr.opcode); w.u16(r.hdr.recordBytes); w.u32(r.hdr.sourceId);
  w.u32(r.hdr.flags); w.zeros(4); // reserved0
  w.u8(r.bloom_gain);
  w.u8(r.flags);
  w.u8(r.flash_amount);
  w.zeros(1); // pad
  w.i16(r.bias_r);
  w.i16(r.bias_g);
  w.i16(r.bias_b);
  zhaoPackRgb565(r.flash, w);
  zhaoPackRgb565(r.ink, w);
  w.zeros(2); // pad_1
}

export function zhaoPackSetGradeTable(r: ZhRecordSetGradeTable, w: ZhByteWriter): void {
  w.u16(r.hdr.opcode); w.u16(r.hdr.recordBytes); w.u32(r.hdr.sourceId);
  w.u32(r.hdr.flags); w.zeros(4); // reserved0
  w.u8(r.curve);
  w.u8(r.first);
  w.u8(r.count);
  w.zeros(1); // pad
  for (let i = 0; i < 72; i++) w.u8(r.vectors[i]!);
  w.zeros(4); // pad_1
}

export function zhaoPackSetPopulation(r: ZhRecordSetPopulation, w: ZhByteWriter): void {
  w.u16(r.hdr.opcode); w.u16(r.hdr.recordBytes); w.u32(r.hdr.sourceId);
  w.u32(r.hdr.flags); w.zeros(4); // reserved0
  w.u32(r.population);
  w.i32(r.origin_x);
  w.i32(r.origin_y);
  w.i32(r.origin_z);
  w.u32(r.active_count);
  w.i32(r.plane_c);
  w.i16(r.plane_nx);
  w.i16(r.plane_ny);
  w.i16(r.plane_nz);
  w.u16(r.flags);
}

export function zhaoPackDebugTraceArm(r: ZhRecordDebugTraceArm, w: ZhByteWriter): void {
  w.u16(r.hdr.opcode); w.u16(r.hdr.recordBytes); w.u32(r.hdr.sourceId);
  w.u32(r.hdr.flags); w.zeros(4); // reserved0
  w.u8(r.stage_mask);
  w.u8(r.flags);
  w.zeros(14); // pad
}

export function zhaoPackDrawPosedForm(r: ZhRecordDrawPosedForm, w: ZhByteWriter): void {
  w.u16(r.hdr.opcode); w.u16(r.hdr.recordBytes); w.u32(r.hdr.sourceId);
  w.u32(r.hdr.flags); w.zeros(4); // reserved0
  w.u32(r.form);
  w.u32(r.material_set);
  w.u32(r.transform);
  w.u8(r.viewport_mask);
  w.u8(r.semantic_weight);
  w.u16(r.flags);
  w.u16(r.clip_id);
  w.u16(r.frame_no);
  w.u8(r.sub);
  w.zeros(11); // pad
}

export function zhaoPackDrawWarpedForm(r: ZhRecordDrawWarpedForm, w: ZhByteWriter): void {
  w.u16(r.hdr.opcode); w.u16(r.hdr.recordBytes); w.u32(r.hdr.sourceId);
  w.u32(r.hdr.flags); w.zeros(4); // reserved0
  w.u32(r.form);
  w.u32(r.material_set);
  w.u32(r.transform);
  w.u8(r.viewport_mask);
  w.u8(r.semantic_weight);
  w.u16(r.flags);
  w.u32(r.warp_program);
  w.u32(r.time);
  for (let i = 0; i < 4; i++) w.fx16(r.params[i]!);
  for (let i = 0; i < 4; i++) w.fx16(r.attributes[i]!);
  w.u32(r.warp_attributes);
  w.u8(r.attribute_mode);
  w.u8(r.warp_flags);
  w.zeros(2); // pad
  for (let i = 0; i < 3; i++) w.fx16(r.displacement_bound[i]!);
  w.zeros(4); // pad_1
}

export function zhaoPackSetPlane(r: ZhRecordSetPlane, w: ZhByteWriter): void {
  w.u16(r.hdr.opcode); w.u16(r.hdr.recordBytes); w.u32(r.hdr.sourceId);
  w.u32(r.hdr.flags); w.zeros(4); // reserved0
  w.u8(r.slot);
  w.u8(r.role);
  w.u8(r.blend);
  w.u8(r.opacity);
  w.u8(r.format);
  w.u8(r.wrap);
  w.u8(r.view_mask);
  w.u8(r.palette_id);
  w.u16(r.width);
  w.u16(r.height);
  w.u16(r.flags);
  w.u16(r.base);
  w.u8(r.lstride);
  w.u8(r.lheight);
  w.zeros(2); // pad
  w.fx16(r.a);
  w.fx16(r.b);
  w.fx16(r.c);
  w.fx16(r.d);
  w.fx16(r.u0);
  w.fx16(r.v0);
  w.fx16(r.line_scroll);
}

export function zhaoPackDrawSprite(r: ZhRecordDrawSprite, w: ZhByteWriter): void {
  w.u16(r.hdr.opcode); w.u16(r.hdr.recordBytes); w.u32(r.hdr.sourceId);
  w.u32(r.hdr.flags); w.zeros(4); // reserved0
  w.i16(r.x);
  w.i16(r.y);
  w.u16(r.w);
  w.u16(r.h);
  w.u16(r.base);
  w.u8(r.lstride);
  w.u8(r.lheight);
  w.u8(r.format);
  w.u8(r.palette_id);
  w.u8(r.blend);
  w.u8(r.view_mask);
  zhaoPackRgb565(r.tint, w);
  w.u8(r.order);
  w.u8(r.flags);
  w.u16(r.src_id);
  w.zeros(2); // pad
  w.fx16(r.u);
  w.fx16(r.v);
  w.fx16(r.a00);
  w.fx16(r.a01);
  w.fx16(r.a10);
  w.fx16(r.a11);
}

// .zcap ABI_INFO identity (capture_format.md 4.2)
export const ZHAO_GENERATOR_NAME = 'zhaozhou-abi-gen';
export const ZHAO_GENERATOR_SHA256: readonly number[] = [0x06, 0xB5, 0xDF, 0x38, 0xE6, 0xC8, 0x38, 0x56, 0x0B, 0xC6, 0x6F, 0x7F, 0xE8, 0xE3, 0x35, 0x99, 0x65, 0xC1, 0xFA, 0x87, 0xB9, 0x1B, 0x90, 0xB9, 0x1A, 0x31, 0x8C, 0xD5, 0x03, 0x73, 0xED, 0xAE];
export const ZHAO_ZIDL_SHA256: readonly number[] = [0x8D, 0xD1, 0x55, 0xE8, 0xE7, 0xEA, 0xBD, 0x59, 0x26, 0xA6, 0x36, 0xB1, 0x4C, 0xA2, 0x41, 0xBB, 0xE6, 0xB6, 0x7C, 0x32, 0xAF, 0xCA, 0x3B, 0x68, 0x22, 0xCD, 0xB7, 0x02, 0x63, 0x28, 0xCB, 0xDA];
export const ZHAO_ZCAP_SCHEMA_VERSION = 1;
