# What the console is waiting on that is not code

2026-09-19. Mandatory gaps **62** (34 tie-offs + 25 disconnected + 3 unbuilt),
down from 78 this morning. Of the 34 tie-off entries, **eleven cannot be closed
by building anything** — they are waiting on a decision.

This is that list, with a recommendation for each and the evidence under it.
Nothing here has been acted on. Where a recommendation is marked **SAFE TO TAKE**
it preserves visible semantics exactly and is reversible in one edit; where it is
marked **OWNER** it changes what the machine does or what the ABI says, and I am
not taking it on my own judgement.

---

## 1. The projector has no depth-profile port — I14 · **SAFE TO TAKE**

`SetView`'s `flags[1:0]` is the depth profile of the frozen 2026-08-31 ruling.
`zhao_project_core` has no port to put it on, so CMD.EXEC lands the camera and
drops the profile.

**Recommend:** add the port, defaulting to `00 = WORLD_LONG`, which
`spec/commands.zidl:301` already ratifies as the zero meaning — *"ZERO KEEPS ITS
MEANING … an existing zero-filled capture still decodes to the profile it was
recorded under."* No existing capture changes. Cost: two bits and a mux.

## 2. A particle client is a THIRD projector port — I24 · **OWNER**

`zhao_proj_subsystem` has exactly two client ports and both are live: GEOM on A,
TERRAIN on B. `zhao_part_project` today time-multiplexes client A, which saved
~6,199 ALM and 33 DSP against a second core. A dedicated third port is an
arbitration change to a block with a verified starvation law.

**Recommend:** leave the time-multiplex, and rule that a third port is not v1.
**Owner's call** because it is a scheduling guarantee: geometry and particles now
share A's bandwidth, and no measured frame says whether that starves either at
the guaranteed content tier. If you want the guarantee, it is a third port and a
new starvation proof.

## 3. TERRAIN.LOD's deviation law — I21/I44 · **OWNER**

The law is derivable from `coarse_height` and `morph_case` with **no new
arithmetic**, and ruling T8 already makes the decimation nested so shared
vertices stay bit-identical. There are **two defensible readings** and they
differ on every subpatch boundary. `dev[L]` has no executable definition anywhere
— its only driver in the tree is an LFSR in the census top.

**Owner's call** because the two readings produce visibly different LOD pops.
This one wants a sentence in `spec/terrain_rules.md`, not an opinion from me.

## 4. MATERIAL.RESOLVE's directory key — **SAFE TO TAKE**

Two thirds smaller than it looked. `zhao_mem_upload` already *takes*
`req_vram_addr_i` and `req_len_i` and bounds-checks both before writing a byte;
it simply drops them on publication, and the kind already travels as
`publish_tag_o`. So base and extent are two output ports carrying values that are
already validated.

What is genuinely missing is **the key**: `dir_set_index_i` wants the 24-bit
handle index, and nothing carries it (`req_tag_i` is 8 bits of "which consumer
asked"; `publish_slot_o` is 8 bits of arena slot).

**Recommend** one sentence in `spec/memory_rules.md` §5f: *a published slot is
named by the handle index of the resource it holds*, making `{index:24}` the
directory key and `{slot, base, extent, kind}` its row. This is the last thing
between the texture island and sampling anything.

## 5. GEOM.LIGHT: two owners for vertex light — **OWNER**

`sky_and_beams.md` §4a assigns vertex light to **GEOM.PROJECT** with a one-sun
rgb565 model. `zhao_geom_light` is an 8-light Q16.16 bank. They cannot both own
it.

And `zhao_light_stream.sv` says *"THIS REPLACES THE OWNER"* and measures the
scalar block at **II = 167.0, 48.1× over frame** for the ruled 120,000-vertex
stress profile.

**Recommend:** rule `zhao_light_stream` the owner and mark `zhao_geom_light`
superseded. **Owner's call** because §4a is a written assignment and overriding
it is your call, not mine. Note this is a supersession **by rename**, a third
shape no tool can currently detect — it is held in `design/console_inventory.yml`
until one can.

## 6. The island handle is the wrong width — **OWNER (contract amendment)**

Ruling T10's handle is `{resource_epoch:u32, slot:u10, generation:u8}` = **50
bits**. `zhao_terrain_island_dir`'s `res_ans_handle_i` is **32**. Confirmed in
four places.

**Recommend:** amend the contract to the 32-bit form actually built, or widen the
port. Either is small; shipping both is not.

## 7. TERRAIN.NORMALS / SHADE: a throughput ruling — **OWNER**

`zhao_terrain_tess.sv:314` declares `ModeTri = 2'd0`; only the sequencer lacks
the arm. `zhao_terrain_pipe.sv:27-34` prices it: a third ModeTri pass is **+456
clocks per level-0 job — "does not fit the two-view schedule"**.

**Owner's call** because the answer is either a schedule change or accepting
flat-shaded terrain normals, and both are visible.

## 8. TEXTURE.TMU: what owns `texture_samples` — **OWNER (ledger)**

`prod_manifest.yml` supersedes TEXTURE.TMU by a *path*, not a module — v3
decomposes the sampler across five live blocks and no single one of them IS the
TMU. Pointing the alias at the planner would be a false reduction. New fact:
`texture_samples` has **no owner at all** — the only `samples_o` in `fpga/rtl` is
`zhao_twod_sampler`'s, which counts 2D compositor texels.

**Recommend:** a ledger decision naming which of the five owns the counter, or
retiring the capability in favour of the path. Over-reporting one gap is the safe
direction until then, and that is what the register does today.

## 9–11. The three "unbuilt" that are optional alternates

Covered in full in `reports/UNBUILT-FOUR-DISPOSITION-20260919.md`: INPUT.SNAC,
GEOM.WARP and POST.ECHO are optional **alternates for functions the console
already performs**, each citing a 2026-08-31 ruling, and each would spend area on
a capability we already have. The ledger records your 2026-09-18 revocation of
that cut, so they are counted as mandatory today. **Owner's call**, and it is the
one with the largest area consequence.

---

## The thing that is not on this list and matters more

**FIELD is not in the 47,582 ALM figure.** That fit's own `.sources.sha256` names
one field file, and `reports/THE-NUMBER-20260919.md` says so in its own words —
the number excludes "FIELD beyond one ROM". FIELD is composed now, so its whole
cost is **additive** to a budget already 5,672 ALM and 39 DSP over, and FIELD at
the *scalar* configuration is charged ~13,700 ALM against a 4,500 ALM envelope.

No ruling fixes that. It is Phase 2 work and it is larger than the headline
suggests.
