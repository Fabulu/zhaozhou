# Packet queue — what fills the next free slot

Ceiling is THREE concurrent packets. When one lands, the next one down starts.
This file is the coordinator's, and it is a WORK LIST: delete a line when the
gap it names is closed, never when it is merely attempted.

**Register at 2026-09-20 morning: 27** = 14 tie-offs + 11 disconnected + 2
unbuilt. Every remaining gap is accounted for below; if a gap is not on this
page, either it has closed or this page is stale, and the register decides.

## Running

| lane | gaps | branch |
|---|---|---|
| FIELD | I5, I34, I42 (R40/R43/R44) | `gz/field` |
| TEXMAT2 | I49, the texture island's first real sample, I20's three `tri_*` shell ports, the `fb_writer_i` substitution hostdbg handed over | `gz/texmat2` |
| GEOMLOD | R68's four sub-builds, then `zhao_geom_parambuf`, `zhao_forge_shadow`, `zhao_forge_prim`, `zhao_forge_prim_eval`, `zhao_forge_cliff`; then R69/I50 and GEOM.WARP | `gz/geomlod` |

## Queued, in the order I would start them

### 1. TERRAIN6 — the largest remaining cluster, and R63 unblocked most of it

Seven gaps: **I21** (TERRAIN.GROUP_SEQ's subpatch job port), **I27** (the
directory's deformation mark and handle check), **I32** (SURFACE.STAMP's
`stamp_results` into TERRAIN.BAKE), and four blocks that are BUILT AND
CONNECTED TO NOTHING: `zhao_terrain_bake`, `zhao_terrain_lod`,
`zhao_terrain_normalmap`, `zhao_terrain_velocity`.

`zhao_terrain_lod` was blocked on a camera position that did not exist. **R63
landed it** — `SetView` now carries `fx16 eye[3]` into the matrix bank's cfg
addresses 18+ — so that blocker is spent. Check it in the tree rather than
taking this sentence for it.

**It also owns R70.** The histogram's v1 metric is now ratified as the terrain
page-load LOD deviation, and `zhao_terrain_loddev` is theirs. Three things the
ruling requires, spelled out in `zhao_console_core.sv`'s I18 entry; the third
is the one that decides whether the work is real: **check that the smoke's
stimulus drives TERRAIN.MIPFEED's fine stream at all** before quoting any
traverse. hostdbg did not verify it and said so.

And **R65 is still owed the owner's eye** — `reports/terrain-seam-dig/
seam_dig_contact.png`. If the half-cell seam step reads as an art defect the
terrain format moves, and the packer does not exist yet, so it is cheaper now
than it will ever be. Do not build the packer around the current format until
that is answered.

### 2. POST3 / MEASURE — two disconnected blocks and one boundary

**I17** (POST.COMPOSITE's gather planes and HUD, `post_gd_*`/`post_gg_*`),
`zhao_post_gather`, and `zhao_measure_governor`. R37's gather law is proposed
and its contact sheet (`reports/post-gather-law/gather_law_contact.png`) is
also waiting on the owner. The governor's `thresh_q8` half went to GEOMLOD
with R68, so read what that packet landed before touching it.

### 3. PROJ / INPUT — the last two clusters

**I13** (PROJ_SUBSYSTEM's triangle output, `proj_out_*`) and **I14** (its
matrix bank, `proj_cfg_*`/`proj_en_i`). R67 says I14 is a FIXTURE AND MODE
decision as much as wiring: the smoke's viewport case has to move to DUO so
the two views get distinct ids and distinct tiles, and the reference-derived
pixel count must be regenerated in the SAME commit with both numbers stated.
I14 also still needs R26's `pixel_error` and a `proj_en_i` producer, so R30
alone does not close it.

**INPUT.SNAC** is NOT BUILT AT ALL — one of the two remaining unbuilt entries
(the other, GEOM.WARP, is GEOMLOD's).

### 4. Whatever the running three refuse

Every packet that refuses a gap must name its exact blocker. Those blockers
are the real queue, and they outrank this list.

## Owed, and cheap only when something else pays the fare

**`spec/commands.zidl`'s DebugTraceArm comment carries an over-broad
guarantee.** It says flatly that the arming record is never traced and that
every record after it is. Neither holds without qualifiers: a SECOND arm in an
already-armed packet is traced, and later records are traced only if the arm
was accepted, the mask sets bit 0, and the ring has room. The corrected
statement is in `zhao_cmd_exec.sv`'s R52 port comment.

It is NOT fixed yet on purpose. Every edit to that file changes
`ZHAO_ZIDL_SHA256`, which forces all five golden captures to be regenerated
through their real producers -- `demo_duo_markers --write` alone is 600 Duo
frames and about an hour. **Whoever next changes the zidl for a real reason
fixes this comment in the same commit**, and the regeneration is free.

## The fit

**Only at zero.** `5CSEBA6U23I7`: 41,910 ALM / 112 DSP / 553 M10K. Nothing in
this run has been fitted, by design — every packet's cost line is an unmeasured
claim until then, and several say so.
