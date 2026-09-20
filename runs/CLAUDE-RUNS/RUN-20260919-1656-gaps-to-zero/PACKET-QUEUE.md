# Packet queue — what fills the next free slot

Ceiling is THREE concurrent packets. When one lands, the next one down starts.
This file is the coordinator's, and it is a WORK LIST: delete a line when the
gap it names is closed, never when it is merely attempted.

**Rewritten 2026-09-20 afternoon. The previous version read 27 and assigned
FORGE and GEOM.WARP to a lane that landed different work — a stale queue is
worse than none, because the next free slot gets filled from fiction.**

**Register: 21** = 9 tie-offs + 11 disconnected + 1 unbuilt. Every one of the
21 is accounted for below. If a gap is not on this page, either it closed or
this page is stale again, and `python tools/budget/completion_register.py`
decides — never this file.

## Running

| lane | what it owns | branch |
|---|---|---|
| FIELDP4 | **not a gap** — ruling R101, a live shipping defect (see below) | `gz/fieldp4` |
| FORGECONNECT | `zhao_forge_shadow`, `zhao_forge_prim`, `zhao_forge_prim_eval`, `zhao_forge_cliff` (4 disconnected) + R94's counter-catalog append | `gz/forgeconnect` |
| TERRAIN9 | `zhao_terrain_normalmap`, `zhao_terrain_velocity`, `zhao_terrain_bake_v2`, `zhao_terrain_lod` (4 disconnected), then I21, I27, I32 | `gz/terrain9` |

**FIELDP4 holds a slot without closing a gap, deliberately.** R101 is a
correctness fault in RTL every Field profile shares: `cur_out_seen == '0'` asks
whether ANY declared output was written, never whether ALL of them were, so a
point writing 5 of 6 declared lanes reports SUCCESS. A silently wrong field
value outranks any remaining gap, because unlike a refusal the consumer cannot
tell. `field_host_directed` case 1 — 1 of 4 lanes, asserting "status is OK" —
is a committed GREEN TEST DOCUMENTING THE BUG. The repair is additive (the
header word's 64 free bits carry a required-output mask; `mask == 0` preserves
today's behaviour exactly), the test must be fixed to assert the CORRECT
behaviour rather than the defect's signature, and the lane must MEASURE which
composed paths under-write today — whether this is producing wrong values now
or only could is a measurement, not an inference.

## Queued, in the order I would start them

### 0. ATTRDIV — a PRE-FIT BLOCKER, and it outranks every gap below

Ruling **R104**. `zhao_raster_attrdiv_svc` and `zhao_raster_attrstep` both wire
`zhao_raster_attrdiv`, superseded by `zhao_raster_attrdiv_v2`. These are the
**last two superseded-in-a-production-root hits** (4 → 2 after R99 retired the
projector shells), and R86 made that check fatal across all 72 roots precisely
so the console cannot be fitted while composing a superseded module. **The fit
cannot honestly run until this closes**, which is why it sits above the gaps.

PROJADOPT refused the swap for the right reason — v1 publishes `rem_o`, both
consumers seed the proven step recurrence with it, and dropping it would delete
function. **But v2 already computes the remainder** (`rem_r [48:0]` at line 79,
the full restoring recurrence at 148–173); it simply does not publish it. The
work is a port and two checks, not new arithmetic:

1. **Width.** v1's `rem_o` is `[47:0]`, v2's `rem_r` is `[48:0]`. Prove the top
   bit is clear at publication BY STIMULUS. The mathematics describes the
   converged value; a port publishes whatever is in the register.
2. **Refusal semantics.** v1 has `q_overflow_o`; v2 has `q_saturated_o` and
   `q_error_o`. v1 REFUSES where v2 SATURATES. Every consumer needs an explicit
   written mapping and a test exercising the overflow path on both sides. This
   is the half that can silently change behaviour.

**Do not re-litigate the rounding.** R100 settled it: v2 matches
`zref::render::div_rhu_s128` exactly over 640,000 sampled pairs; v1 disagrees on
100% of negative exact halves with an even divisor. Adopting v2 is a bug fix.

### 1. POST3B / MEASURE — two disconnected blocks and one boundary

**I17** (POST.COMPOSITE's gather planes and HUD, `post_gd_*`/`post_gg_*`),
`zhao_post_gather`, `zhao_measure_governor`.

**`zhao_post_gather` is blocked on the owner and on nothing else.** R37's
gather law is proposed and `reports/post-gather-law/gather_law_contact.png` is
waiting on the owner's eye. Do not work around it; if the block cannot close,
refuse it and say so.

`zhao_measure_governor` moved under this run: R26/R68 gave it the per-camera
pixel-error threshold, R83/R98 widened that to **Q12.8** (Q8.8 saturated at
255.996 while the real value reaches 443.41), and its latency is now 77
clocks/frame, up from 69. Read what PROJADOPT landed before touching it.

### 2. PROJ — I13 and I14, and the ground under them just moved

**I13** (PROJ_SUBSYSTEM's triangle output, `proj_out_*`) and **I14** (its
matrix bank, `proj_cfg_*`/`proj_en_i`).

**PROJADOPT changed what these sit on.** Under R99 `zhao_prod_top` stopped
instantiating the two standalone projector shells (`zhao_geom_project`,
`zhao_terrain_project` — still in the tree, still selected tops, ~12,267 ALM /
66 DSP between them) and now instantiates the composition the console core
actually holds: `zhao_proj_subsystem` + `zhao_geom_proj_lane`. Re-read the
ports against the current top, not against any earlier note.

R67 says **I14 is a FIXTURE AND MODE decision as much as wiring**: the smoke's
viewport case has to move to DUO so the two views get distinct ids and distinct
tiles, and the reference-derived pixel count must be regenerated **in the same
commit with both numbers stated**. I14 also still needs a `proj_en_i` producer,
so R30 alone does not close it.

### 3. GEOM — I20, I29, and `zhao_geom_parambuf`

**I29** (GEOM.POSE's clip page and skeleton bake, `geom_pose_start_i`),
**I20** (everything `zhao_shell_top_v2` still declares provisional at its own
boundary), and `zhao_geom_parambuf` (disconnected).

I20's texture half closed with I49 — the island now samples, 1190 fragments
carried a texel. What remains of I20 is named rather than left to be
re-derived: `base_rgb` is the VERTEX's colour and GEOM.VATTR holds a per-vertex
one, so the flat base colour is still a named constant;
`tri_continuation_tail_i` and `tri_fragment_state_i` are still boundary ports;
and a CLUT material's palette slot and generation **have no producer in this
console at all**, which `mat_win_clut_unowned_o` counts rather than hides.

### 4. I34 — TERRAIN.PATCH's field-height lane

`terr_pt_fld_*` and its section 9.1. It is field-adjacent; do not start it
while FIELDP4 is live in the same files.

### 5. GEOM.WARP — the one unbuilt entry, and it is WAITING, not neglected

Architected and ratified on 2026-09-20 as W01–W18 from the owner's
`reports/Zhaozhou_GEOM_WARP_Architecture_2026-09-20.txt` (commit `4c256137`),
dated that day and citing that commit — nothing back-dated, because the file
self-describes as a proposal. `zref::geom_warp` is BUILT (102 directed checks,
3 mutants fired), replacing item 9 of `PHANTOM_REFERENCES.md`.

**It composes nothing, deliberately.** Tying `zhao_geom_warp`'s Field port off
would convert an honestly-absent entry into a tie-off, which is the move rule 1
forbids. **R103 names nine shared Field prerequisites that do not exist**, and
this entry cannot close until they do. Its cost is 51+T_run clocks/point
against Earth's 49. Do not start this lane before those nine have a home.

### 6. Whatever the running three refuse

Every packet that refuses a gap must name its exact blocker. Those blockers are
the real queue, and they outrank this list.

## Owed to the OWNER — both now blocking a register entry

1. **`reports/post-gather-law/gather_law_contact.png`** (R37). `zhao_post_gather`
   is blocked on nothing else.
2. **`reports/terrain-seam-dig/seam_dig_contact.png`** (R65) — and **the render
   changed the question**. Spec §9.3(c) says the half-cell step "does not read
   as a seam"; the sheet shows a rim wrong by up to one vertex, everywhere. If
   that reads as an art defect the terrain FORMAT moves, and the packer does not
   exist yet, so it is cheaper now than it will ever be.

Three more decisions came out of PROJADOPT: funding the one-packet `rem_o`
repair that R100 is really blocked on; seven unresolved `reference_model:` rows
(five of them PART.*) that need a per-row call; and the FORGE.SHADOW
counter-catalog append, which FORGECONNECT is now carrying.

## Owed, and cheap only when something else pays the fare

**`spec/commands.zidl`'s DebugTraceArm comment carries an over-broad
guarantee.** It says flatly that the arming record is never traced and that
every record after it is. Neither holds without qualifiers: a SECOND arm in an
already-armed packet IS traced, and later records are traced only if the arm was
accepted, the mask sets bit 0, and the ring has room. The corrected statement is
in `zhao_cmd_exec.sv`'s R52 port comment. **R77's `tmu_mode` comment is queued
the same way.**

Not fixed yet on purpose: every edit to that file changes `ZHAO_ZIDL_SHA256`,
which forces all five golden captures to be regenerated through their real
producers — `demo_duo_markers --write` alone is 600 Duo frames and about an
hour. **Whoever next changes the zidl for a real reason fixes both comments in
the same commit**, and the regeneration is free.

## The fit

**Only at zero.** Then TWO runs, per R80: the verdict on the target
`5CSEBA6U23I7` (41,910 ALM / 112 DSP / 553 M10K) and the map on the sizing
device `5CEBA9F31C7`, because at roughly 113% ALM the target will likely refuse
to place and a refusal is not a map.

The only composed number that exists is `zhao_console_core@console-core-first-light`
— **47,582 ALM / 151 DSP / 306 M10K**, fitted on the sizing device ONLY to
measure size, `gpu_clk` 18.5 MHz, setup slack −44.06 ns, TNS −39,647 ns, and
`treeCleanAtHead: false`. Every packet's cost line in this run is an unmeasured
claim until the real fit, and several of them say so.

**Read `rtlCleanAtHead` before `status` on any fit row.** A row stamped
`failed:structure` is not a failed measurement — the fit completed and the
budget rules rejected it. A row fitted from a dirty tree, whose digest describes
nothing, can be stamped `ok`. A gate reading `status` alone refuses the
trustworthy number and quotes the worthless one.
