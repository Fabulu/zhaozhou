<!-- TRANSCRIBED FROM COMMIT MESSAGES, 2026-09-21.

The harness refuses a lane-written FINDINGS file, so this packet landed its
record as commit message(s). The text below is REPRODUCED VERBATIM from
935245ab, 890449d2 -- it is the lane's own words, not the
coordinator's summary of them. Cite this file; the commits remain the primary.
-->

# FINDINGS -- PROJOUT


## `935245ab` -- PROJOUT: I13 re-measured -- two of its own sentences are false. Register 22 -> 22

```
PROJOUT: I13 re-measured -- two of its own sentences are false. Register 22 -> 22

FINDINGS-projout.md: the harness REFUSED the lane's write to the run folder, as
the brief predicted. Findings are in this commit message; coordinator transcribes.

Comment-only. No RTL, no port change, no new module. 103 lines added to entry
I13 in fpga/rtl/prod/zhao_console_core.sv. Register 22 before and 22 after
(9 tie-offs + 13 disconnected). Phantom-gap trap checked: no added line matches
the register's entry-header pattern.

I13 AND I14 DO NOT SHARE A PRODUCER. Not re-litigated -- CFGARM measured it
hours earlier. Verified only the fact I13 rests on: u_geom_clip.tri_valid_i is
cl_in_valid, and cl_in_valid = mw_t_valid && !cl_in_refuse_c. One producer
chain. Nothing I13 needs is a CMD record or a CMD executor.

THE BLOCKERS, RE-MEASURED
  (a) two-producer merge into GEOM.CLIP  STANDS -- and is LARGER than stated
  (b) invw24                             STANDS -- only its COSTING was wrong
  (b) u/w, v/w                           SPENT (R197), verified in RTL
  (b) lit r/g/b                          NOT AN ATTRIBUTE BLOCKER -- it moved
  (b) alpha                              SPENT (R48): MAT_BASE_ALPHA_C is 8 bits of ones

FALSE SENTENCE 1 -- "a terrain triangle entering tri_* has to fill all of it".
SLOTS 3,4,5,6 (lit r, lit g, lit b, alpha) HAVE NO READER ANYWHERE IN fpga/rtl.
Three independent structural confirmations, each read by hand:
  * zhao_geom_attrpack is the packet's only consumer and packs THREE planes;
    design/contracts/GEOM.CLIP.md calls it "the ONLY reader" of slots 1 and 2.
  * zhao_shell_top_v2 declares exactly three 240-bit plane inputs:
    tri_invw_plane_i, tri_u_over_w_plane_i, tri_v_over_w_plane_i.
  * zhao_raster_tile_pipe_v2 joins exactly three attribute lanes
    (attr_join_q_q[0..2]) and takes colour from base_rgb off the 298-bit FLAT
    request instead -- flat_request_q[43:20], per triangle, from the material
    window.
So the attribute packet costs terrain EXACTLY ONE SLOT: invw24.

R225 PAID FOR ITSELF INSIDE THIS MEASUREMENT. A grep for ATTRS returned
thirteen files and looked like a wall of consumers. Every one outside the five
real ones matches on the SUBSTRING inside ATTRSETUP and ATTRSTEP. The
uniform-looking result was the instrument. One file opened by hand settled it.

AND THE MOVE THAT FINDING INVITES IS REFUSED IN ADVANCE, in the entry. "Four
slots nobody reads" reads like 384 bits to delete by setting GEOM_CLIP_ATTRS to
3. That is closing the distance to a gap by deleting the place the answer lands.
THE SLOTS ARE EMPTY, NOT SPARE.

FALSE SENTENCE 2 -- "the merge in (a) is a day's work once (b) exists".
u_material_window sits IN the stream between GEOM.REPLAY and GEOM.CLIP and its
correctness argument is structural. A second producer at GEOM.CLIP's input has
three obligations NO ENTRY NAMES:
 1. it must fire d_enter_i. That port is cl_in_valid && cl_in_ready -- the
    window's OWN triangle. A merged triangle entering without it still departs
    on d_leave_i, driving occupancy_q below zero; the block fires
    err_occupancy_underflow_o and CLAMPS THE COUNTER AT ZERO, which makes
    drained_c read TRUE while triangles are in flight. The interlock's drain
    condition becomes a lie -- the exact fault that header says it prevents.
 2. it must carry a material_set and material_id pair. The window publishes ONE
    material for the whole span and shades every triangle in it with that.
    TERRAIN HAS NEITHER FIELD: a search of fpga/rtl/terrain/ for material_set
    and material_id returns ZERO hits. Terrain textures through the MOSAIC path
    -- tileset plus the layer-E mat_a/mat_b/weight triple the projector
    forwards unselected. A different mechanism with a different key, NOT a
    material record with a missing producer.
 3. err_unpublished_o's premise -- "a triangle may only reach the door through
    this block" -- is falsified BY CONSTRUCTION by any second door.
None of this argues against R187's ruling that the honest door is GEOM.CLIP's
input. IT IS THE BILL FOR THAT RULING, AND NOBODY HAD ADDED IT UP.

THE ONE INSTRUMENT CHECKED RATHER THAN ASSUMED: obligation 1's detector is not
blind. err_occupancy_underflow_o is FIRED BY STIMULUS at
tests/texture/material_window_directed.cpp case 7, asserted EXACTLY 1, with
case 5's asserted 0 as its negative control. I looked before writing a mutant
for it; the mutant was not owed.

invw24 -- ABSENT, ONLY ITS COSTING WAS WRONG. Not a second
zhao_geom_depthquant_stream beside a second zhao_raster_rcp24_v4. That block is
TAG-THROUGH (v_tag_i to d_tag_o, TAGW wide) over a pool of NSLOT = 16 contexts,
so a second client is an arbiter on v_* and a demux on d_* keyed by a client bit
in the tag. Costing it as a second instance would repeat the projector exactly;
design/contracts/TERRAIN.PROJECT.md holds that receipt in its own words: "it is
also not a DSP saving: both shells hold their own core, so the pair is still 66,
map-measured." WHETHER THE SHARED INSTANCE HAS HEADROOM IS UNMEASURED, and it is
a Verilator question at 2,000 terrain triangles/frame, NOT a fit question.

THE OWNER DECISION -- and it is smaller than the one this entry was carrying.
The art law is not where I13 puts it. Re-pointing lit r/g/b at the base-colour
seam was right; INHERITING THAT SEAM'S REFUSAL WAS NOT. That refusal is written
against the MESH path and says so: GEOM.VATTR holds a per-VERTEX rgb under R11,
the flat request wants one colour for the triangle, and "picking one of the three
corners would be an art decision made by a composer."
THAT OBJECTION DOES NOT APPLY TO TERRAIN. zref::render::shade_flat_tri is flat
PER TRIANGLE by construction and terr_light_base_o is one signed scalar for the
same reason. THERE IS NO CORNER TO PICK.
And the composition law already exists in the oracle, in
reference/src/zrender/terrain.cpp:
  * untextured profile: lit(base) = (base * shade + 32768) >> 16, per channel,
    against the material's mat.r/g/b;
  * textured profile: mod_of(shade, tint, sheet) -- the palette ladder
    (shade + 8191) >> 14, then ONE rounding over the s128 product
    shade x tint x sheet, with ALL-UNITY EXACT (the reference states the
    exactness itself).
So the question is NOT "what colour is terrain", which would be art. It is WHICH
OF THE TWO RATIFIED PROFILES THE CONSOLE RUNS -- and, if the second, who
produces tint (layer H) and sheet.
RECOMMENDATION: the TEXTURED profile with tint and sheet at unity, on R221's own
reasoning one step over. R221 ruled the ST_MISS fallback to the parametric disc
because "the disc is not an invention -- it is the RATIFIED v1 LAW", and
all-unity mod_of is ratified and bit-exact by the reference's own statement. It
reaches behaviour that already exists rather than inventing a third, and leaves
tint and sheet as knobs their producers can later drive -- CLAUDE.md rule 6
rather than a stub. NOT TAKEN HERE: R197 explicitly declined terrain art content.

WHAT I DELIBERATELY DID NOT DO. No build; every candidate fails a standing rule,
measured rather than assumed:
  * terrain DEPTHQUANT alone -- lands nowhere until the merge exists. A
    BUILT-INSTALLED-NOWHERE uncashed cheque, which uncashed_cheques.py gates on.
  * the merge alone -- falsifies the material window's interlock and shades
    terrain with the mesh's published material. Closing a gap by opening a worse.
  * narrowing GEOM_CLIP_ATTRS -- refused in the entry.
  * a mutant for err_occupancy_underflow_o -- already fired by stimulus.
NO MANUFACTURED REGISTER RISE. zhao_terrain_lightlane is in
design/prod_manifest.yml but has NO design/blocks.yml row and NO contract under
design/contracts/; there is no contract for a terrain-to-GEOM.CLIP merge either.
Per R219's distinction -- a row is honest when the capability is ALREADY
SPECIFIED -- inventing one here would inflate the denominator to have something
to declare. An honest 22 beats a dishonest 23.

FOR THE HANDOVER. I13's real remaining shape is ONE absent slot, ONE
ruled-but-unbilled door, and ONE owner sentence:
 1. invw24 -- engineering; the cheap shape is a second tagged client.
 2. the merge at GEOM.CLIP's input -- R187 ruled the door; the material window's
    three obligations are the unpaid bill, and obligation 2 is the deep one.
 3. which ratified lit-colour profile terrain runs -- recommended, not taken.
Blockers (b)'s u/w, v/w and alpha should leave the board for good.

THE PROCESS FINDING. This entry has been re-measured four times in three days
and its central sentence about the attribute packet had never been checked
against the CONSUMERS -- only against the packet's own declared width. READING A
LAYOUT IS NOT READING A READER. The correction moved the work in BOTH directions
at once, which is the tell that nobody had measured it: a citation that has only
rotted gets smaller; one nobody checked gets smaller in one place and larger in
another.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
```

## `890449d2` -- PROJOUT: the console computes per-vertex colour and throws it away -- and that is why I13's slots are empty

```
PROJOUT: the console computes per-vertex colour and throws it away -- and that
is why I13's slots are empty

Comment-only, inside the block added by 935245ab. Register 22 -> 22, quartus17
syntax RC 0, tie-off audit 0 SILENT, console inventory OK. This is the lane's
best finding and it corrects my OWN recommendation from two commits ago.

THE CHAIN, MEASURED END TO END.
  zhao_geom_light        produces lit r/g/b PER VERTEX
  zhao_geom_vattr        stores it per vertex (owner ruling R11)
  rp_attr_a/b/c          slots 3..5 of the seven-slot packet
  zhao_geom_clip         carries them, winding-flipped with their corners
  zhao_geom_attrpack     packs THREE planes -- AND IT IS DROPPED HERE
  frag_vert_rgb_i        port comment: "interpolated, lit, tinted, FOGGED"
                         driven from returned_retire_ctx_w.raster_continuation
                         .post_earlyz.vertex_rgb
                         <- job_meta_i[345:298]
                         <- tri_continuation_tail_i, a 48-bit PER-TRIANGLE
                            constant with NO PRODUCER (entry I20)

A per-vertex quantity is computed, carried four blocks, and delivered as a
per-triangle constant that nothing drives. THAT is why slots 3..5 have no
reader -- not because the colour path is absent, but because it was built flat
while the carriage was built per-vertex.

AND IT CORRECTS MY OWN COMMIT 935245ab. I recommended terrain's colour ride the
FLAT base_rgb seam, reasoning that terrain's shade is flat per triangle so there
is no corner to pick. That is TRUE OF THE SHADE AND FALSE OF THE TINT:
  * spec/terrain_rules.md 6.5, quoted verbatim inside zhao_texture_aux.sv: the
    aux budget holds "ONE aux consumer on terrain fragments, because TINT MOVED
    TO VERTICES." Layer-H tint is PER-VERTEX by ratified spec.
  * and the oracle says the same about itself -- terrain.cpp introduces its
    per-cell tint as "the FLAT STAND-IN for the Gouraud tint". A stand-in that
    names itself one.
Recommending the flat route would have ratified the stand-in as the design,
which is this repository's own standing warning about composing the
simplification that happens to exist. The correction is left in the entry
beside the wrong version rather than replacing it, because the correction is
worth more than the tidiness.

AND IT REMOVES ONE OF R224's FOUR "NOTHING TO DERIVE FROM" CLAIMS. That ruling
searched spec/*.zidl and spec/*.md for vertex_rgb, vertex_alpha, effect_tag and
stencil_reference, got ZERO hits, and concluded tri_continuation_tail_i is
ABSENT DATA. The search was right; a LITERAL-TOKEN search cannot see
terrain_rules 6.5, because that sentence says "tint moved to vertices" and never
writes the field's name. For terrain the derivation exists: per-vertex layer-H
tint modulated by the flat shade terr_light_base_o already leaves this module
with. THIS DOES NOT OVERTURN R224 -- the other three tail fields are untouched
and terrain is one primitive class of several. It removes exactly one claim, and
it is the one I13 depends on.

THE CORRECTED QUESTION FOR THE OWNER. Terrain's colour is PER-VERTEX by spec.
Its destination is either GEOM.CLIP slots 3..5 -- which needs GEOM.ATTRPACK to
grow from three planes to six, real silicon on a device at 97% -- or the
continuation tail's vertex_rgb, which needs I20's producer. IF THE FLAT STAND-IN
IS CHOSEN ANYWAY, and it may well be the right call on the ALM budget, IT MUST
BE NAMED A STAND-IN IN THE RTL with terrain_rules 6.5 cited beside it, or the
next reader inherits a Gouraud law silently implemented as a constant.

METHOD NOTE. Three of this lane's four findings came from asking "who READS
this?" rather than "does this EXIST?". The entry's own searches were all of the
second kind and all honest; the packet layout, the coordinate law and the colour
law were each verified present or absent and none was traced to a consumer.
READING A LAYOUT IS NOT READING A READER, and it is the cheaper of the two
searches, which is presumably why it is the one that keeps getting done.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
```
