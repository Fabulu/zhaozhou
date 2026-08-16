# Zhaozhou Cartridge — `.zpak` Container

**Status:** new spec, wave 3 (plan W3.1, decision D8; FORM §3 ".zpak resource
pages"; charter §23 Phase 3 "cartridge packer"). The cartridge is a game
loaded like software, never a resynthesized machine (FORM §2): field
microprograms, generated ARM code manifests, source maps, costs and asset
pages travel as **data**.

**Law:** the container reuses the `.zcap` section discipline **verbatim**
(spec/capture_format.md §4.1/§4.2/§4.3/§4.4 — byte-order rules, CRC-32C
parameterization §2, section-table layout, evolution rules, reader/writer
discipline). Only the magic, the section-type vocabulary and the
RESOURCE_PAGES kind registry differ; everything else is the same bytes under
the same laws. Where this file is silent, capture_format.md is normative.

---

## 1. File layout

Identical to capture_format §4.1 with the magic and one reserved bit of
identity:

```
Offset  Size  Field
0       4     magic 'Z','P','A','K'  (u32 LE 0x4B41505A)
4       2     format_version = 1
6       2     flags               bit0 = little_endian (must be 1); bits 1-15 reserved 0
8       4     header_crc32c       CRC-32C over bytes [0,8)
12      4     section_count       u32
16      4     section_table_offset = 32
20      4     section_entry_size = 32
24      8     total_file_length   u64
32      ..    section table: section_count × 32-B entries (capture_format §4.1)
```

Section-table entries, per-section CRC-32C over the body, no whole-file CRC
(partial recovery law), unknown-section skipping, per-type section_version
evolution, ABI_INFO-first ordering, duplicate-section error: **all
capture_format §4.1–§4.4 verbatim.** A `.zpak` reader is a `.zcap` reader
with a different magic and a section-type table; one implementation serves
both (tools/pack, W3.6).

**Trailer:** `total_file_length` in the header plus the last section's
`body_offset + body_length` MUST equal the file size exactly — the packer
asserts it; a mismatch is a corrupt cartridge (checked before any section
body is trusted, mirroring the fail-safe order of capture_format §3.2).

## 2. Section types

| Type | Name | Contents |
|---|---|---|
| 0x0001 | ABI_INFO | identical shape to .zcap (capture_format §4.2): `u32 abi_version; u32 schema_version; u8 generator_name[16]; u8 generator_sha256[32]; u8 zidl_sha256[32]` — must be first. `generator_name` = `"zhaozhou-pack"`; the SHA-256 identities pin the .zidl and the packer's canonical manifest text exactly as in .zcap. |
| 0x0002 | PROGRAM | one serialized field program `.zprog` body (field-ir.md §5) — one section per program, page_id = section index in manifest order |
| 0x0003 | RESOURCE_PAGES | `u32 count` + count × the exact .zcap RESOURCE_PAGES record (§3 below) |
| 0x0004 | SOURCE_MAP | the binary `sourceids.zmap` body (capture_format §7) |
| 0x0005 | CODE_MANIFEST | generated-code manifest (§4) |
| 0x0006 | TERRAIN_PATCH | authored heightfield patch page (§4) |
| 0x0007 | SKY_SET | sky asset set page (§4) |
| 0x0008 | TONE_BANK | tone bank page (§4) |
| 0x0009 | COSTS | `costs.zcost` body verbatim (spec/form/cost-model.md §2) |
| 0x000A | TERRAIN_ISLAND | island patch page v1 (§4; format law spec/terrain_rules.md §2) |
| 0x000B | ISLAND_TABLE | island directory (§4; spec/terrain_rules.md §1.5) |
| 0x000C | CREATURE_FORM | compiled creature form page (§4; spec/creature_rules.md §5 kind 8) |
| 0x000D | CLIP_BANK | animation clip bank page (§4; spec/creature_rules.md §5 kind 9) |
| 0x8000-0xFFFF | tool namespace | tools may add private sections; readers MUST skip (capture_format §4.3-1) |

FRAME_PACKET sections do not belong in a cartridge (a cartridge is not a
capture); a reader that finds one skips it under the unknown/skip law or
diagnoses it as a packing error — the packer never emits one.

## 3. RESOURCE_PAGES record (verbatim reuse, D8)

The record shape is the .zcap RESOURCE_PAGES record **exactly**
(capture_format §4.2 type 0x0003):

```
u32 count
count × { u8 kind; u8 rsv[3]; u32 page_id; u64 byte_length;
          u8 sha256[32]; u8 ref[64] }
```

`rsv` must be zero; `sha256` is over the page body (the section body the
entry describes); `ref` is a NUL-padded UTF-8 reference string (source path
or symbolic name). `page_id` is the value Form source references as a
page-id constant (language-semantics §5); `kind` selects the page family:

| kind | Family | Backing section | Contents |
|---|---|---|---|
| 0 | field program | PROGRAM | a `.zprog` (field-ir §5) + its C++ wrapper identity |
| 1 | sourceids.zmap | SOURCE_MAP | the binary source map (capture_format §7) |
| 2 | generated-code manifest | CODE_MANIFEST | §4 below |
| 3 | sky set | SKY_SET | per spec/sky_and_beams.md asset set (bands/cap/under/clouds/sun) |
| 4 | terrain patch | TERRAIN_PATCH | authored heightfield patch (§4) |
| 5 | tone bank | TONE_BANK | wave-2 mixer tone set (spec/audio_rules.md lane) |
| 6 | island patch | TERRAIN_ISLAND | Island Patch v1 page (spec/terrain_rules.md §2 — layered top/bottom/state/material/sheet/tint) |
| 7 | island table | ISLAND_TABLE | island directory: datum, pitch_log2, grid extent, tileset, sparse patch map (spec/terrain_rules.md §1.5) |
| 8 | creature form | CREATURE_FORM | compiled parts→meshlets, bones ≤32, attachments, hitboxes, ladder refs (spec/creature_rules.md §5) |
| 9 | clip bank | CLIP_BANK | 30 Hz quantized-quat clips + keyframe event tags (spec/creature_rules.md §2.1) |

~~Kinds 6-255 reserved~~ ~~Kinds 8-255 reserved~~ Kinds 10-255 reserved
(world-identity wave, RUN-20260816-0046, added kinds 6/7 then 8/9); a reader
that meets an unknown kind skips the page
(fail-safe, never guesses). The packer cross-checks every Form page-id
constant against this table at pack time (FORM-E-830/831,
language-semantics §8).

## 4. Page families

- **Field programs (kind 0):** the compiled `.zprog` bytes (field-ir §5:
  header, tables, code, map — byte-stable, hash-asserted). The program hash
  (CRC-32C over code+tables, field-ir §5.4) is recorded in the CODE_MANIFEST
  so a load can refuse a program whose bytes drifted.
- **sourceids.zmap (kind 1):** the binary source map, format per
  capture_format §7 (magic ZSMP). One per cartridge; page_id fixed at 1.
- **Generated-code manifest (kind 2):** canonical JSON (the cost-model §2
  canonicalization law) listing the generated C++ artifacts with SHA-256
  per file: `{"abi_version":2,"files":[{"name":"form_game.hpp","sha256":"…"},
  …],"programs":[{"name":"rising_ridge","hash_crc32c":…,"profile":"earth"}]}`.
  The ARM/desktop runtime verifies the manifest before linking the
  cartridge's generated entry points — generated code is committed and
  byte-stable (charter §29 ground rules), and this page is the pin.
- **Sky set (kind 3):** per spec/sky_and_beams.md — L1 emits no sky
  statements (domains-and-effects §4); the renderer's clear path consumes
  the set directly. The page shape is the sky spec's own asset layout.
- **Terrain patch (kind 4):** an authored heightfield patch: header
  `{u16 width; u16 height; fx16 x0, z0, x1, z1 (rectfx envelope); u16 rsv[6]}`
  followed by `width × height` × height16 (s16, qformats §2/§9 — bake-back
  rounding law) in ascending z-then-x order. Patch dimensions are bounded by
  the terrain patch budget line when Phase 0 pins it; the packer rejects an
  odd-sized or empty patch deterministically.
  **[world-identity wave] Kind 4 is the Phase-3 bootstrap page (single
  surface, no rim topology) and stays valid for existing captures/demos; new
  Phase-6+ world content ships as kind 6 below. Kind 4 gains no new
  features.**
- **Island patch (kind 6):** one Island Patch v1 page, byte layout normative
  in `spec/terrain_rules.md` §2 (64-B header + layers A–H, 21,320 B body;
  the VRAM stride pad is a residency artifact and is NOT stored). Ascending
  z-then-x within every lattice/cell plane, same as kind 4. The packer
  asserts header/envelope redundancy and the page CRC (terrain_rules §2.1).
- **Island table (kind 7):** island directory per `spec/terrain_rules.md`
  §1.5: `{u32 island_count}` + records `{u32 island_id; fx16 origin_x,
  origin_y_datum, origin_z; i8 pitch_log2; u8 rsv[3]; u16 grid_w, grid_h;
  u32 tileset_id}` followed by each island's sparse patch map
  `{u32 entry_count} + entry_count × {i16 ix; i16 iz; u32 page_id}` (page_id
  names a kind-6 page). One table per cartridge.
- **Creature form (kind 8) / clip bank (kind 9):** semantic contents per
  `spec/creature_rules.md` §5 (form: parts→meshlet ids, bone hierarchy,
  attachments, hitboxes, ladder refs; bank: clip directory + 30 Hz frames of
  root fx16[3] + s16[4] quantized quats + event tags). Byte-exact layouts
  freeze with SW.TOOLS.ASSET at Phase-12 entry (creature_rules §9); until
  then the packer refuses to emit them (deterministic refusal, never a
  guessed layout).
- **Tone bank (kind 5):** the wave-2 mixer tone set the EmitAudioEvent path
  consumes (MixerTone records; spec/audio_rules.md): one header
  `{u32 tone_count}` + tone records `{u32 event_id; u16 gain; i16 pan;
  fx16 pitch; u8 sample_index; u8 rsv[3]; u32 rsv2}`. `event_id` matches the
  EmitAudioEvent `event_id` field; a missing id at play time is a mixer-level
  drop, never a truth change (FORM §15).

## 5. Packing discipline (tools/pack, W3.6)

- **Deterministic:** sections are written in a fixed order (~~ABI_INFO, then
  PROGRAM pages in source-ID order, SOURCE_MAP, CODE_MANIFEST, SKY_SET,
  TERRAIN_PATCH pages in page-id order, TONE_BANK, COSTS, RESOURCE_PAGES
  last~~ ABI_INFO, then PROGRAM pages in source-ID order, SOURCE_MAP,
  CODE_MANIFEST, SKY_SET, TERRAIN_PATCH pages in page-id order,
  ISLAND_TABLE, TERRAIN_ISLAND pages in page-id order, TONE_BANK, COSTS,
  RESOURCE_PAGES last — world-identity wave insertion, additive), bodies
  written sequentially, table and header backpatched — the
  .zcap writer discipline (capture_format §4.4). Two packs of one build are
  byte-identical (`pack:check` staleness gate; same law as abi:check).
- **Round-trip:** pack → load must reproduce the identical program bytes,
  source map and costs (byte compare) — the W3.6 acceptance test.
- **Integrity:** every section carries CRC-32C; page entries additionally
  carry SHA-256; the loader verifies per-section CRC before trusting any
  body, and CODE_MANIFEST hashes before executing generated entry points.
- **`abi_version` pinning:** ABI_INFO carries the ABI version; a loader
  built against a different `version` refuses the cartridge
  (`ZH_ABI_BAD_ABI_VERSION` semantics at cartridge level) rather than
  guessing (capture_format §4.3-2 law).
