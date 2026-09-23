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
| 0x000E | TEXTURE_PAGE | generic sampled bytes + interpretation (§4a; owner ruling D-2, 2026-09-03) |
| 0x000F | MATERIAL_SET | generic immutable material table indexed by `material_id` (§4a) |
| 0x0010 | MESH_STREAM | generic meshlet descriptors + vertex + local-index streams (§4a) |
| 0x0011 | SPECIES_TABLE | particle species descriptor table (§4b; owner ruling R42, 2026-09-19) |
| 0x0012 | FORGE_PROGRAM | Primitive Forge program table (§4d; owner decision R234 D2, 2026-09-21) |
| 0x0013 | TWOD_PAGE | compositor 2D page: texels or palette entries for the TWOD sampler's on-chip store (§4f; owner ruling 2026-09-22 item 3) |
| 0x0014 | DETAIL_NORMAL | terrain detail-normal pyramid: a header then the flat seven-level signed {dx, dz} pyramid TERRAIN.NORMALMAP's resident tile holds (§4g; owner ruling R243 D-NORMALS-A, 2026-09-23) |
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
| 10 | texture page | TEXTURE_PAGE | immutable sampled bytes: dimensions, format, mip count/offsets, strides, texel payload, integrity identity, optional palette subtype (§4a) |
| 11 | material set | MATERIAL_SET | immutable table indexed by `material_id`: 0–3 sample bindings, page handles, TMU state, combiner recipe/weight, raster state, cel/ink participation, fog exemption, AUX use (§4a) |
| 12 | mesh stream | MESH_STREAM | immutable geometry: meshlet descriptors, vertex stream, local-index stream, offsets/counts, format + generation metadata (§4a) |
| 13 | species table | SPECIES_TABLE | the particle species descriptor table: a header then PART.TABLE load words, one per entry (§4b; owner ruling R42, 2026-09-19) |
| 14 | forge program | FORGE_PROGRAM | the Primitive Forge program table: a header then one 192-byte record per program — family, subdivision and the anchors/axes/radii its evaluator places vertices from (§4d; owner decision R234 D2, 2026-09-21) |
| 15 | twod page | TWOD_PAGE | compositor 2D page: a raw run of little-endian 16-bit words, either CLUT8/RGB565 texels or RGB565 palette entries, loaded into the TWOD sampler's on-chip store. `dst_slot` names the destination region (§4f; owner ruling 2026-09-22 item 3) |
| 16 | detail normal | DETAIL_NORMAL | TERRAIN.NORMALMAP's resident detail tile and its mip tail: a 64-byte header then `words` little-endian `u16` texels `{s8 dz, s8 dx}`, in `zref::terrain::normalmap_pyramid_addr` order, read into the block's on-chip pyramid by `zhao_terrain_normalloader` (§4g; owner ruling R243 D-NORMALS-A, 2026-09-23) |

~~Kinds 6-255 reserved~~ ~~Kinds 8-255 reserved~~ ~~Kinds 10-255 reserved~~
~~Kinds 13-255 reserved~~ ~~Kinds 14-255 reserved~~ ~~Kinds 15-255 reserved~~
~~Kinds 16-255 reserved~~ Kinds 17-255 reserved
(world-identity wave, RUN-20260816-0046, added kinds 6/7 then 8/9); a reader
that meets an unknown kind skips the page
(fail-safe, never guesses). The packer cross-checks every Form page-id
constant against this table at pack time (FORM-E-830/831,
language-semantics §8).

## 4. Page families


## §4a — The three generic resource families (owner ruling D-2, 2026-09-03)

**Ruled after `reports/BORING_3D_FUNDAMENTALS_AUDIT.md` R4 found that this
registry had no generic texture-page or material-set kind while three
subsystems already assumed one**: creature parts claim texture pages, terrain
names tilesets, and `DrawForm` carries a `material_set` handle.

Three options were considered (per-family embedding; a generic blob plus typed
manifests; generic families). **Generic families was ruled**, because it is the
only one under which `MEM.UPLOAD` can be a single general mover for every
immutable render asset, and the only one where the texture cache's invalidate
has one obvious publisher.

### The ownership rule, which is the point of the ruling

    family resource
       |- references MESH_STREAM
       |- references MATERIAL_SET
       '- MATERIAL_SET references TEXTURE_PAGE

**`CREATURE_FORM`, `SKY_SET`, terrain tilesets and future object-form pages MAY
reference these, and MAY NOT embed a second family-specific interpretation of
the same texture, material or mesh concepts.** Family pages become
**manifests, not private loaders**.

`MEM.UPLOAD` copies all three as **opaque immutable bytes** under one residency,
generation, integrity and publication law. **It does not need to know whether
the bytes depict Zixx, a cliff, a water surface or a fireball** — and that is
the property that makes one uploader sufficient.

### Palettes

Palette data is a **subtype of `TEXTURE_PAGE`**, not a separate family. It uses
the same publication and generation machinery rather than growing an unrelated
loader path — which is also what lets D-3's generation-tagged cache coherence
cover palettes without a special case.

### What is NOT frozen here

The **record layouts** inside each family. `MATERIAL_SET`'s entry shape is
drafted in `design/contracts/MATERIAL.RESOLVE.md` and is explicitly not frozen
by that file either. **The ABI generator owns the final emitted constants and
regeneration**; these table rows allocate the kinds and section types, which is
what unblocks the loader and the uploader.

- **Field programs (kind 0):** the compiled `.zprog` bytes (field-ir §5:
  header, tables, code, map — byte-stable, hash-asserted). The program hash
  (CRC-32C over code+tables, field-ir §5.4) is recorded in the CODE_MANIFEST
  so a load can refuse a program whose bytes drifted.
- **sourceids.zmap (kind 1):** the binary source map, format per
  capture_format §7 (magic ZSMP). One per cartridge; page_id fixed at 1. Its
  complete body is subject to capture_format §7.4's inclusive 134,217,728-byte
  (128-MiB) v1 ceiling. The `.zpak` section table's u64 body length does not
  widen that source-map law.
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
  guessed layout) — **except kind 8's HEADER and LADDER TABLE, which owner
  ruling R26 (2026-09-19) unfroze and §4c freezes now.**
- **Tone bank (kind 5):** the wave-2 mixer tone set the EmitAudioEvent path
  consumes (MixerTone records; spec/audio_rules.md): one header
  `{u32 tone_count}` + tone records `{u32 event_id; u16 gain; i16 pan;
  fx16 pitch; u8 sample_index; u8 rsv[3]; u32 rsv2}`. `event_id` matches the
  EmitAudioEvent `event_id` field; a missing id at play time is a mixer-level
  drop, never a truth change (FORM §15).

## 4b - SPECIES_TABLE (owner ruling R42, 2026-09-19)

The particle species descriptor table, as a page. Core entry I33 refused to
invent a command that carries a species descriptor, and it was right to:
`reference/include/zref/zref_particle.hpp` says "there is no species table
... That is a DATA/ABI question and it is properly the owner's". R42 makes the
descriptors DATA instead - authored in a page, published through
`PublishResource`, and read into `PART.TABLE` by
`fpga/rtl/particles/zhao_part_table_loader.sv`.

WHAT IS FROZEN HERE IS THE ENVELOPE, NOT A SPECIES. An entry carries
`zhao_part_table`'s OWN load word, unchanged and uninterpreted: the four
selector slices that block already accepts, {sel, index, event, data}. Nothing
in the console reads a field inside `data`; the contents remain the author's.

Everything is 64-byte shaped because `MEM.GUARD`'s read is at most 64 bytes
and its shape rule requires the byte mask to match the length, so a reader that
asks for whole lines is the simplest one that can be correct. An entry is 32
bytes rather than the 19 its fields need, so two fit a line exactly and no
entry ever straddles a read.

`
HEADER - 64 bytes, one line, at the page's base
  u32 magic     'ZSPT'  (0x5450535A little-endian on the wire)
  u16 version   1
  u16 entries   how many entries follow
  u8  rsv[56]   zero

ENTRY - 32 bytes, TWO per line, starting at byte 64
  bit   1:0   sel     0 UPD, 1 COL, 2 SPW, 3 CRV
  bit   3:2   event   SPW only: 0 birth, 1 mark, 2 collision, 3 death
  bit   7:4   rsv     zero
  bit  14:8   index   species, or curve bucket in its low four bits
  bit  31:15  rsv     zero
  bit 172:32  data    the LD_W-wide load word, LSB-aligned (LD_W = 141 at the
                      console's widths: 12 + 2*AGE_W + 5*VEL_W + 3*POS_W)
  bit 255:173 rsv     zero
`

A page is REFUSED WHOLE on a wrong magic, a wrong version, or an `entries`
count that runs past the length the publication declared. It is never
partially loaded: a half-loaded species table is a particle engine running on a
mixture of two authors' physics, which reads as a tuning problem and is not one.

Model: `reference/include/zref/zref_species_page.hpp` (`build` and
`decode`). Hardware: `zhao_part_table_loader`, differenced against it in
`tests/particles/part_table_loader_directed.cpp`.

## §4c — CREATURE_FORM's LADDER TABLE (owner rulings R26 / R68, 2026-09-19)

**A PARTIAL lift of the kind-8 freeze, for four fields and no others.** R26:
*"Lift the kind-8 freeze for GEOM.LOD's FOUR constants only (bound radius,
micro/splat/glint error). Their layout is frozen now and the packer emits
them."* R68 schedules it as its own packet.

`fpga/rtl/geometry/zhao_geom_lod.sv` — the creature representation ladder — is
built, unit-verified and fit. Four of its five inputs are these constants, and
`fpga/rtl/prod/zhao_console_core.sv` traced FORGE.SHADOW's whole refusal to
them: *"There is no layout to read because the project has ruled that there
must not be one yet."* This section is the ruling that ends that.

**What is frozen here is the HEADER and the LADDER TABLE. Nothing else.**
Parts, meshlet ids, the bone hierarchy, attachments and hitboxes remain frozen
until SW.TOOLS.ASSET at Phase-12 entry, exactly as §4 says. The header's
`body_off` names the byte offset at which that body will begin, so the
unfrozen half can be appended later without moving a byte of the frozen half;
`body_off == 0` means "no body in this page", which is what the packer emits
until SW.TOOLS.ASSET exists. Passing a nonzero `body_off` to a packer that
writes no body is **refused**.

Everything is 64-byte shaped for the reason §4b gives: `MEM.GUARD`'s read is at
most 64 bytes and its shape rule requires the byte mask to match the length, so
a reader that asks for whole lines is the simplest one that can be correct. A
record is 32 bytes rather than the 20 its fields need, so two fit a line
exactly and no record ever straddles a read.

```
HEADER - 64 bytes, one line, at the page's base
  u32 magic      'ZCFM'  (0x4D46435A little-endian on the wire)
  u16 version    1
  u16 records    how many ladder records follow
  u32 body_off   byte offset of the Phase-12 body, or 0 if absent
  u8  rsv[52]    zero

LADDER RECORD - 32 bytes, TWO per line, starting at byte 64
  bytes  0..3   u32 form_index    MESH_STREAM handle index, bits 23:0;
                                  bits 31:24 MUST be zero
  bytes  4..7   i32 bound_radius  fx16 world metres, > 0
  bytes  8..11  i32 micro_error   fx16, >= 0
  bytes 12..15  i32 splat_error   fx16, >= 0
  bytes 16..19  i32 glint_error   fx16, >= 0
  bytes 20..31  rsv[12]           zero
```

**The key is the MESH_STREAM handle index, not an invented type id.**
`zref::creature::CreatureType::type_id` is software's own key and no hardware
port carries it; `DrawForm.form` is a `handle32` whose index
`zhao_geom_drawjob` already keys its MESH_STREAM residency directory by
(rule §5f.1). Two instances of one creature share that index, so it IS the
per-creature-type key at the hardware boundary — and using it means the ladder
bank and the mesh directory agree by construction rather than by a second
mapping law (the same decision R45 made for the stamp's patch).

A page is **REFUSED WHOLE** on a wrong magic, a wrong version, a `records`
count that runs past the length the publication declared, a count larger than
the reader's row capacity, or **any illegal record** — a `bound_radius` at or
below zero, a negative error, or a nonzero high byte in `form_index`. It is
never partially loaded: a bank holding four rows of one author's creature and
the rest of another's reads as an art problem and is not one, and
`zref::creature::lod_raw`'s divide-free identities hold only for a positive
bound radius and non-negative errors.

Model: `reference/include/zref/zref_creature_page.hpp` (`build`, `decode`,
`record_legal`, `lookup`). Packer: `tools/pack/mkcreatureladder.py`, whose
`--check` rebuilds the committed golden
`tests/golden/creature_ladder/ladder_page_v1.bin`. Hardware:
`fpga/rtl/geometry/zhao_geom_ladderbank.sv`, differenced against the model and
required to read that same golden in
`tests/geometry/geom_ladderbank_directed.cpp`.

## §4d — FORGE_PROGRAM (owner decision R234 D2, 2026-09-21)

**The Primitive Forge's program page, and the reversal of a deferral.**

R199 deferred this format, and its reason was good: *"four of the six forge
families have no evaluator at all … a page ruling buys one of six"*, and a
format frozen ahead of its consumers is a format frozen on guesses. **Owner
decision R234 D2 reverses it** — *"the owner has chosen to pay for the
evaluators rather than accept the deferral. The page kind is to be frozen and
FORGE.PRIM / FORGE.PRIM_EVAL built."* The evaluators are being built in the same
pass, so this layout is frozen against consumers that exist.

`DrawProcedural 0x0302` carries `handle32[forge_program] program` and states
that *"forge parameters do NOT travel inline"*. Until this section,
`forge_program` named a resource type occurring in exactly **two** places in the
whole tree — that command and its generated ABI table. This is the format the
handle points at.

### What a program is, and why it is ONE record

A forge program is one primitive: its **topology** (which family, how finely
subdivided) and its **positions** (the anchors, axes and radii the evaluator
places vertices from). `zhao_forge_prim` owns the first and the evaluators own
the second; they are the two halves of one meshlet, joined only by the
ring-major ordering convention. Splitting them across two pages would create a
second mapping law between them and buy nothing.

**The key is the `handle32` index**, bits 31:8 of `DrawProcedural.program`, and
nothing else — the same decision R26 took for the ladder table's `form_index`
and R45 took for the stamp's patch. No second id space, so nothing can disagree.

### The family is in the page, and the command's `kind` must AGREE

A ribbon's parameters and a tube's parameters are not the same parameters, so a
reader cannot interpret a record without knowing its family: **the family
belongs in the page**. `DrawProcedural.kind` declares it a second time, and the
two are authored in different places — the page by the packer, the command by
the game's own draw — so comparing them is a real check rather than two
operands moving together.

**RULED: the page's `family` governs. A `kind` that disagrees REFUSES the draw
and is counted** — FORGE.PRIM's own "refuse, never substitute a similar one",
applied to a disagreement about a known family.

**And the comparison is a ROTATION.** `spec/commands.zidl` says in capitals that
`forge_kind` is *not* `zhao_forge_prim`'s `j_family_i` encoding, because member 0
there is frozen by the v2 pad-byte precedent:

    forge_kind = (family + 1) mod 6

*"A straight-through assignment is silently wrong for all six values."*
`zref::forge_page::kind_of_family` / `family_of_kind` are the **one** place that
conversion is written; `tests/forge/forge_page_directed.cpp` walks all six both
ways. Nobody writes the arithmetic a second time.

### Layout

Everything is 64-byte shaped for the reason §4b and §4c give: `MEM.GUARD`'s read
is at most 64 bytes and its shape rule requires the byte mask to match the
length, so a reader that asks for whole lines is the simplest one that can be
correct. Those two pages chose a 32-byte record so **two** fit a line. A forge
program does not fit in 32 bytes and no packing will make it — the ribbon alone
needs five fx16 three-vectors, four fx16 scalars, a seed, a phase and two branch
descriptors. So a record is **192 bytes = THREE whole lines**. The property that
matters is not "two per line", it is **no record ever straddles a read**, and
every multiple of 64 has it.

```
HEADER - 64 bytes, one line, at the page's base
  u32 magic      'ZFPG'  (0x4746505A little-endian on the wire)
  u16 version    1
  u16 records    how many program records follow
  u8  rsv[56]    zero

PROGRAM RECORD - 192 bytes, THREE lines, starting at byte 64

  LINE 0 - identity, topology and the common frame
  bytes   0..3   u32 program_index  handle32 index, bits 23:0;
                                    bits 31:24 MUST be zero
  byte    4      u8  family         the SILICON encoding, 0..5
                                    (zhao_forge_prim's FAM_*, NOT forge_kind)
  byte    5      u8  sweep          0 LINEAR, 1 DOME (DOME on FAM_SHELL only)
  byte    6      u8  segments       1..64 (1..24 on FAM_RIBBON)
  byte    7      u8  sides          1..8; MUST be 1 on an open family
  byte    8      u8  view_mask      bits 1:0, nonzero; bits 7:2 MUST be zero
  byte    9      u8  branch_count   0..2; MUST be 0 off FAM_RIBBON
  bytes  10..11  u16 src_id
  bytes  12..15  rsv[4]             zero
  bytes  16..27  i32 anchor0[3]     fx16 — the centre of ring 0
  bytes  28..39  i32 anchor1[3]     fx16 — the centre of ring N
  bytes  40..51  i32 axis_u[3]      fx16 — ring U axis / ribbon width axis
  bytes  52..63  i32 axis_v[3]      fx16 — ring V axis / ribbon jitter axis 1

  LINE 1 - radii, and the ribbon's jitter law
  bytes  64..67  i32 radius0        fx16, >= 0 — R(0), or the ribbon half width
  bytes  68..71  i32 radius1        fx16, >= 0 — R(N); MUST equal radius0
                                    on FAM_RIBBON
  bytes  72..75  i32 amp            fx16 — ribbon jitter amplitude
  bytes  76..79  i32 branch_amp     fx16
  bytes  80..83  i32 branch_radius  fx16 — branch half width
  bytes  84..87  u32 seed
  bytes  88..89  u16 tick_phase_base
  bytes  90..91  rsv[2]             zero
  bytes  92..103 i32 axis_w[3]      fx16 — ribbon jitter axis 2; zero elsewhere
  bytes 104..127 rsv[24]            zero

  LINE 2 - the ribbon's branches
  byte  128      u8  br0_attach     0..segments
  byte  129      u8  br0_segments   1..8 when branch_count > 0
  bytes 130..131 rsv[2]             zero
  bytes 132..143 i32 br0_end[3]     fx16
  byte  144      u8  br1_attach
  byte  145      u8  br1_segments
  bytes 146..147 rsv[2]             zero
  bytes 148..159 i32 br1_end[3]     fx16
  bytes 160..191 rsv[32]            zero
```

**The three axes are a FRAME, used as supplied.** FORGE.PRIM.EVAL rules that
deriving a unit frame in hardware *"needs a square root and a divider this block
has no business owning"*, and the CPU computes it once per effect anyway. The
ring families use `axis_u` and `axis_v`; the ribbon uses all three — `axis_u` as
its width axis, `axis_v` and `axis_w` as its two jitter axes. That is one frame,
not an overload of two.

**`sweep` is the law between the anchors**, and it exists so the radial shell can
be a dome as well as a cone without a seventh family:

* `LINEAR` — centre and radius both lerp. Tubes, cones, fans, sheets, ribbons.
* `DOME` — centre and radius follow the frozen `SIN_Q16` quarter wave, a dome or
  bowl. **Legal on `FAM_SHELL` only**; allowing it elsewhere would make `sweep`
  a second, silent family selector.

### A page is REFUSED WHOLE

On a wrong magic, a wrong version, a nonzero reserved byte, a `records` count
that runs past the length the publication declared, a count larger than the
reader's row capacity, two records sharing a `program_index`, or **any illegal
record** — a nonzero high byte in `program_index`, a family above 5, a sweep
above 1 or a DOME off the shell, a subdivision of zero or past its family's cap,
`sides != 1` on an open family, a zero or over-wide `view_mask`, a negative
radius, a branch descriptor outside its caps, or a ribbon-only field set on a
family that has no ribbon law.

It is never partially loaded. `zref::species_page`'s sentence is true here for
the same reason: a bank holding three programs of one author's effect and the
rest of another's reads as a tuning problem and is not one.

### Two declared holes, named so they are not read as oversights

1. **A ribbon's two radii MUST be equal.** `zhao_forge_prim_eval`'s job carries
   one `half_width`, so a tapering ribbon is a capability the evaluator does not
   have. The page **refuses** `radius0 != radius1` on `FAM_RIBBON` rather than
   carrying a number nothing reads. When a tapering ribbon is built the refusal
   lifts and not one byte moves — the same "declared hole" discipline as §4c's
   `body_off`.

2. **`tick_phase_base` is a BASE, not the live phase.** A cartridge page is
   immutable and uploaded once; a bolt animates per frame. The evaluator's
   `tick_phase` is `tick_phase_base + frame_tick`, and **`frame_tick` is sourced
   at DISPATCH, not from here.** Where the dispatch gets it is an **open owner
   question** — `DrawProcedural`'s `pad[11]` could carry it under the same
   mandatory-zero reinterpretation `forge_kind` itself used, or the console
   could broadcast its frame sequence. **This section does not decide it**, and
   with `frame_tick == 0` a page alone is a complete, deterministic, static
   primitive.

Model: `reference/include/zref/zref_forge_page.hpp` (`build`, `decode`,
`record_legal`, `lookup`, `kind_of_family`, `family_of_kind`). Packer:
`tools/pack/mkforgeprogram.py`, whose `--check` rebuilds the committed golden
`tests/golden/forge_program/forge_page_v1.bin`. The golden is also read by
`tests/forge/forge_page_directed.cpp`, so packer, model and that one artefact
are pinned to each other rather than to good intentions — a layout edit that
misses one of them goes red.

**No hardware reads this page yet, and that is stated rather than implied.** The
staging path is the terrain pattern (`zhao_terrain_pageloader` moves a body,
`zhao_terrain_hdrread` turns its header into registers) and it is named as
remaining work in `design/contracts/FORGE.PRIM.md`. Freezing the format is what
R234 D2 authorised; building its reader is the next packet.

**And §5's deterministic section ORDER is not extended here, deliberately.** That
list names eleven section types and the registry in §2 now holds eighteen —
`CREATURE_FORM`, `CLIP_BANK`, `TEXTURE_PAGE`, `MATERIAL_SET`, `MESH_STREAM` and
`SPECIES_TABLE` were all absent from it before this section existed, so
`FORGE_PROGRAM` is the seventh and not the first. Choosing a position is a
ruling about pack determinism, and `tools/pack` has no forge writer to be
constrained by it yet; picking one here would be freezing a second thing nobody
asked for, in a section whose whole subject is what R199 said about freezing
ahead of consumers. **Whoever writes the forge packer takes that position and
extends §5 for all seven at once.**

## §4e — FORM OWNERSHIP in the BODY and the CLIP_BANK (owner ruling, 2026-09-21)

**Ruling:** `reports/Zhaozhou_kind8_kind9_proposed_owner_ruling_2026-09-21.txt`.
It resolves the cardinality question §4c's `body_off` left open — *one kind-8
page is an N-form ladder table plus at most ONE creature's skeleton, and
nothing said whose* — and the identical absence one page kind over, where a
`CLIP_BANK` header carried no form key at all.

**Why an absence rather than a bug.** A resident skeleton and a resident clip
bank could answer any draw. Bone counts agree in the common case, so a
mismatch detector reading zero was telling the truth about the wrong quantity,
and the result is a **well-formed palette for the wrong animal with every
counter green**. The ruling's fix is a field, not a heuristic: *"Bone count,
loader order, first ladder row, physical slot, and equality of publication
indices are NOT proofs of form ownership."*

### The bytes

One aligned little-endian `u32` storage word per semantic `u24`. **Bits 23:0
are the owner's MESH_STREAM form index; bits 31:24 MUST be zero** and a page
setting them is REFUSED, never masked — masking would make two stored words
mean one form.

```
BODY header (TCB8), version 2     bytes 16..19   u32 owner_form_index
CLIP_BANK header (ZCLP), version 2 bytes 20..23   u32 owner_form_index
```

**Both headers remain 64 bytes.** The words went into existing padding, so no
page grew, no bone record, clip record or frame stride moved, `body_off` keeps
its §4c meaning, and the reader needs no additional header transaction — its
read was already a whole 64-byte line.

**Index zero is not a sentinel.** Presence of an owner follows the VERSION and
the validated presence of the section, never the numeric value. A body or bank
owned by form 0 is an ordinary one and is served.

### The versions are SPLIT, not bumped

**`BODY` and `CLIP_BANK` go to version 2. The outer kind-8 `ZCFM` header and
§4c's ladder record stay at version 1.** A reader must therefore check three
versions separately; `zhao_geom_clipread` had one shared `VERSION` constant and
now has `FORM_VERSION` / `BODY_VERSION` / `CLIP_VERSION`, because in the
ruling's words *"merely changing that single constant to 2 would reject the
still-v1 outer header."*

**A bodyless outer-v1 ladder page stays legal** — §4c's `body_off == 0` is
unchanged and raises no fault.

**Unowned v1 body or clip data is NOT accepted on the posed-render path.** An
offline conversion needs a supplied, validated owner; it cannot discover one
from bytes that never held it. The historical v1 goldens stay committed and
stay identifiable, and the reader reads them and refuses them.

### What a packer must do, and must not

The owner is **supplied by the asset definition or manifest**. It is never
inferred from row zero of the ladder, from a matching bone count, from the
publication index or from loader order — none of those is evidence, and a
packer has no honest way to guess.

**A body-bearing kind-8 page must contain a LADDER RECORD for its body's
owner.** The other ladder records remain independently owned metadata; they are
**not** users of that body. §4c's ladder lookup and whole-page replacement
semantics are unchanged, and the bank stays a multi-form table.

### What the loader must prove

Before a pose is decoded, published or drawn: the request names a valid,
generation-checked MESH_STREAM form; body and clip resources are completely
adopted; `request.form_index == body.owner_form_index`; `request.form_index ==
clip.owner_form_index`; the bone counts agree; the clip, frame and sub-phase
are legal. **A mismatch is refused and counted, never treated as a cache miss.**
The same condition protects cache hits, not only miss decoding.

**Data, ownership and validity are adopted as one logical state.** An owner
read out of a header is staged and committed only with its payload — publishing
it earlier leaves a load that is later denied with the previous section still
resident wearing the new page's identity.

### Where it is stated, and what pins it

Model: `reference/include/zref/zref_creature_page.hpp` (`body::kOffOwnerForm`,
`body::build_body`, `body::decode_body`, `body_owner_in_ladder`,
`build_with_body`) and `reference/include/zref/zref_clip_page.hpp`
(`kOffOwnerForm`, `build`, `decode`). Packers: `tools/pack/mkcreatureladder.py
--body-owner` and `tools/pack/mkclipbank.py --owner`, both REQUIRED, both with
`--check`. Goldens: `tests/golden/creature_ladder/ladder_page_body_v2.bin` and
`tests/golden/creature_clip/clip_page_v2.bin`, whose owner is the ladder's
**second** row on purpose, so "read the owner word" and "read row zero" are
separable measurements. Hardware: `fpga/rtl/geometry/zhao_geom_clipread.sv`
(`p_form_idx_i`, `res_body_owner_o`, `res_clip_owner_o`, `owner_mismatch_o`),
fed by `fpga/rtl/geometry/zhao_geom_drawjob.sv`'s `j_form_idx_o`. Evidence:
`tests/geometry/geom_clipread_directed.cpp`, `tests/geometry/clip_page_directed.cpp`
and the positive control `tests/mutants/zhao_geom_clipread_ownerblind_mutant.sv`.

### Ownership is not resource discovery

Two owner fields make the association **verifiable**. They do not tell a loader
which of several nonresident pages to fetch, and they do not establish a
multi-body directory or a multi-resident skeleton cache. The
one-body/one-directory staging tier is unchanged and is still the measured one.

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
  Packers and loaders apply the source-map structural/u32/128-MiB checks of
  capture_format §7.4 before allocating its complete body or narrowing any
  u64 container offset or length to a host index.
- **`abi_version` pinning:** ABI_INFO carries the ABI version; a loader
  built against a different `version` refuses the cartridge
  (`ZH_ABI_BAD_ABI_VERSION` semantics at cartridge level) rather than
  guessing (capture_format §4.3-2 law).

## §4f — TWOD_PAGE (owner ruling 2026-09-22, item 3)

**Ruled by the completion ruling's asset clause**, quoted because it is what
makes this section part of item 3 rather than a new decision:

> *"This includes any required legitimate asset/palette loading producer. **An
> opcode plus descriptors referring to data that only the testbench can inject
> is not completion.** Reuse `PublishResource` and the existing validated
> resource mechanisms wherever applicable."*

`SetPlane` and `DrawSprite` name a region of the **TWOD sampler's on-chip page
store** (`fpga/rtl/compositor/zhao_twod_sampler.sv`) by `base`, `lstride` and
`lheight`. Something has to put bytes there, and before this kind existed the
only writer was a testbench hanging on `twod_ld_*` at the console's edge.

### The body is a raw run of 16-bit words, and that is deliberate

A TWOD_PAGE body is `length` bytes of **little-endian `u16` words** and nothing
else — no header, no magic, no dimension fields.

**The page carries no geometry because the DESCRIPTOR already does.** Width,
height, stride and format are `SetPlane`/`DrawSprite` fields, judged by the
consumer that owns the refusal. A header repeating them would be a second
opinion about the same facts, and `spec/cartridge.md` §4d already records what
that costs — *"the page's `family` governs; a `kind` that disagrees REFUSES the
draw"*. There is nothing here to disagree.

### `dst_slot` names the destination region

`PublishResource.dst_slot` selects where the words land. The map is the
sampler's own store, divided at its existing boundaries:

| `dst_slot` | destination | words |
|---|---|---|
| 0..7 | page-store slot *s*, first word `s * (PAGE_WORDS/8)` | `PAGE_WORDS/8` |
| 16..19 | palette slot 0..3 | 256 |
| anything else | **refused and counted** (`slot_refused_o`) | — |

At the shipped `PAGE_WORDS = 8192` a page slot is 1,024 words (2 KiB) and a
palette slot is 256 RGB565 entries. **A transfer longer than its slot is
refused whole and counted** (`len_refused_o`) rather than being allowed to walk
into the neighbouring slot — the same fail-safe direction as every other
bound-check in this file.

### Every other `PublishResource` law is unchanged

`length` is a multiple of 64 (a refusal, never a pad), `crc32c` is
`zhao_crc32c_fold`'s law over the staged bytes, `epoch` is checked against the
open resource epoch, and `hps_addr_lo/hi` above 4 GiB is refused rather than
narrowed. **TWOD_PAGE adds no new integrity mechanism**; it adds a destination.

**A page whose CRC fails is not published.** `TWOD.ASSET`'s loader ZEROES the
destination region it had begun writing and counts the failure, so a bad
transfer leaves a defined region rather than a partial one. See
`design/contracts/TWOD.ASSET.md`.

### It does NOT travel to VRAM

Kinds 0..14 are staged into local SDRAM by `MEM.UPLOAD`. Kind 15 is read
straight out of the HPS arena into on-chip memory by `TWOD.ASSET`, because the
sampler's store is on-chip and *"there is no texel page store in the tree that a
(u, v) can walk into without a VRAM fill agent"* — the wall
`zhao_console_core.sv` entry I17 names, and the reason that block carries a
page store of its own rather than a memory client. `CMD.EXEC` forks the upload
request by `kind`: 15 goes to `TWOD.ASSET`, everything else to `MEM.UPLOAD`,
and no existing kind changes route.

## §4g — DETAIL_NORMAL (owner ruling R243 D-NORMALS-A, 2026-09-23)

**Ruled in four words** — *"In v1 — commission the pyramid."*

`fpga/rtl/terrain/zhao_terrain_normalmap.sv` has held a seven-level signed
detail pyramid (`tile_m [0:PYR_WORDS-1]`, 5,461 words) and a write-only upload
port (`tw_we_i` / `tw_addr_i` / `tw_data_i`) since it was rebuilt to contract on
2026-09-09. It is instantiated **nowhere**. `design/prod_manifest.yml` carries
it as an open deferral, and every reference to the file in `fpga/rtl/` is a
comment citing it as an M10K-inference precedent.

The reason is not the block. **Nothing in the tree could put bytes in that
pyramid.** A module with a resident tile and no producer is the shape the
completion ruling's asset clause names: *"An opcode plus descriptors referring
to data that only the testbench can inject is not completion. Reuse
`PublishResource` and the existing validated resource mechanisms wherever
applicable."* This kind is that reuse. **No new command opcode exists for it**
— `PublishResource 0x0030` (R17) already carries every field, and three blocks
already select their own publications by a `PAGE_KIND` parameter
(`zhao_geom_ladderbank` 8, `zhao_part_table_loader` 13, `zhao_forge_pagebank`
14). A fourth joins them.

### The bytes

```
HEADER — 64 bytes, one line, at the page's base
  u32 magic     'ZDNM'  (0x4D4E445A little-endian on the wire)
  u16 version   1
  u16 words     pyramid words that follow, 1..5461
  u8  rsv[56]   zero

BODY — `words` × u16, THIRTY-TWO per line, starting at byte 64
  each word  {s8 dz, s8 dx}   dx in bits 7:0, dz in bits 15:8
  in `zref::terrain::normalmap_pyramid_addr` order, flat word 0 first
```

Everything is 64-byte shaped for the reason §4b gives and no other:
**`MEM.GUARD`'s read is at most 64 bytes and its shape rule requires the byte
mask to match the length**, so the simplest reader that can be correct is one
that asks for whole lines. A word is 2 bytes and a line is 64, so a line holds
exactly 32 words and **no word ever straddles a read**. There is no padding
inside the body; the page as a whole is padded to a multiple of 64 because
`MEM.UPLOAD`'s length rule requires that of every upload — a refusal, never a
pad, on the command side.

The word is the hardware's upload word unchanged. `zhao_terrain_normalmap`'s
`tw_data_i` is `{s8 dz, s8 dx}` and `tw_addr_i` is a flat 13-bit word address;
the loader hands over one and the other, and interprets neither.

### The header carries NO level count, and that is deliberate

The pyramid's level structure is `normalmap_pyramid_addr`'s law and the
hardware's `LEVELS` parameter. A `levels` field would be a **second opinion
about the same fact**, and §4d already records what a second opinion costs
— *"the page's `family` governs; a `kind` that disagrees REFUSES the draw"*.
There is nothing here to disagree, and the loader forms no view about which
level a word belongs to: it carries a flat word to a flat address.

`words` is a COUNT, not a structure claim, and it is the field §4b's `entries`
is. A page with fewer words than the hardware's `PYR_WORDS` loads a prefix; the
block ignores writes at or beyond `PYR_WORDS` by its own rule (*"an upload-tool
fault, not a machine state"*), so a page built for a deeper pyramid than the
silicon carries is truncated by the silicon rather than refused by it.

### The pyramid is built OFFLINE, by averaging SIGNED dx/dz, NEVER normalising

`zhao_terrain_normalmap.sv` and the mipmapping addendum
(`reports/zhaozhou-terrain-mipmapping-architecture-2026-09-05.txt` §4) both say
this in the same words, and it is a LAW of the page rather than a hint to the
packer, because the hardware has no way to check it.

`s*dot(d, L)` is **linear in `d`**, so the average of four texels'
contributions is the contribution of their average. Re-normalising each level
would break that linearity and — worse — would give a flat region
(`dx = dz = 0`, the correct answer for *"no relief here"*) an arbitrary unit
direction, so a surface that should go quiet at distance would instead acquire
a constant tilt. **Coarsening detail must converge to nothing**, and signed
averaging is what makes it: a lone 127 spike decays 127, 32, 8, 2, 1, 0, 0
across the seven levels.

Rounding is **half away from zero**. The symmetry is load-bearing: negating a
detail tile must negate its whole pyramid exactly, or a relief and its mirror
image would coarsen differently and a mirrored piece of terrain would shimmer
where its twin did not.

### A page is REFUSED WHOLE

On a wrong magic, a wrong version, a `words` above the layout's 5,461, or a
`words` that runs past the extent the publication declared. It is never
partially loaded: a half-loaded pyramid is terrain whose relief changes at a
mip boundary for no authored reason, which reads as an aliasing bug and is not
one. A publication arriving while a load runs is **dropped and counted**, not
queued, for the reason §4b gives for SPECIES_TABLE.

### It travels the ordinary route

Kind 16 is staged into local SDRAM by `MEM.UPLOAD` exactly as kinds 0..14 are,
and the loader reads it back through the ENGINE1 asset window. **Kind 15
remains the only kind `CMD.EXEC` forks**, and no existing kind changes route.
`length` is a multiple of 64, `crc32c` is `zhao_crc32c_fold`'s law over the
staged bytes, `epoch` is checked against the open resource epoch, and
`hps_addr_lo/hi` above 4 GiB is refused rather than narrowed. **DETAIL_NORMAL
adds no new integrity mechanism**; it adds a destination.

Model: `reference/include/zref/zref_normal_page.hpp` (`build_pyramid`, `build`
and `decode`). Packer: `tools/pack/mkdetailnormal.py`, pinned with the model
and the hardware reader to the one committed artefact
`tests/golden/detail_normal/detail_normal_v1.bin`. Hardware:
`fpga/rtl/terrain/zhao_terrain_normalloader.sv`.
