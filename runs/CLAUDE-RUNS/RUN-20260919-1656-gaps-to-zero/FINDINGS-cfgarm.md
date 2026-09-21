<!-- TRANSCRIBED FROM COMMIT MESSAGES, 2026-09-21.

The harness refuses a lane-written FINDINGS file, so this packet landed its
record as commit message(s). The text below is REPRODUCED VERBATIM from
f056e586, 5db18f12, 926df70f, e1b4cbd7, e0ab6551 -- it is the lane's own words, not the
coordinator's summary of them. Cite this file; the commits remain the primary.
-->

# FINDINGS -- CFGARM


## `f056e586` -- CFGARM 1/n: the tree at HEAD could not configure, and the gate that should have caught it reads one of two generated files

```
CFGARM 1/n: the tree at HEAD could not configure, and the gate that should have caught it reads one of two generated files

`cmake --preset windows-native` FAILS at aea45c4a for every lane, with seven
`%Error-PINMISSING` on `tests/mutants/zhao_shell_paired_diff_mutant.sv`: the
`gth_*` POST.GATHER group (`gth_valid_o`, `gth_rgb565_o`, `gth_tag_o`,
`gth_addr_o`, `gth_x_o`, `gth_y_o`, `gth_last_o`) landed on
`zhao_shell_top_v2` at d3f90937, the non-mutant harness was regenerated at
6b1fe2a0, and the MUTANT was left at ce25a260 -- before the ports existed.

THE INSTRUMENT IS WHY IT SURVIVED, and it is the usual direction.
`gen_shell_paired_diff.py` writes TWO files, OUT and MUT. `--check` read only
OUT. The gate list in PACKET-PROTOCOL.md and in every packet brief runs the
bare `--check`, so the gate printed "fresh" while the file it does not read
could not elaborate. `mutant_copy_drift.py` does not cover it either: that
tool reads provenance for HAND-CUT copies and this copy is GENERATED, so no
gate owned the question at all. Neither tool was wrong about its own subject;
between them there was a file nobody was looking at.

THE REPAIR IS STRUCTURAL, not a regeneration:
  * a bare `--check` now checks BOTH files, so the form everybody already runs
    cannot go blind again;
  * the STALE message named the bare command for both paths, so a reader whose
    mutant was stale was told to rewrite the file that was already fresh -- a
    repair instruction that reports success and changes nothing. It now names
    `--mutant` when that is the stale one;
  * `tests/mutants/zhao_shell_paired_diff_mutant.sv` regenerated.

PROVEN BY STIMULUS, in this order, not argued: the repaired gate FAILS on the
pre-repair tree naming the mutant, PASSES after the regeneration, and
`cmake --preset windows-native` goes RC=1 -> RC=0 across the same change.

Pre-existing and NOT mine, recorded so it is not read as caused here:
`mutant_copy_drift.py` is RC=1 at aea45c4a on
`zhao_geom_bonesrc_latefetch_mutant`.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
```

## `5db18f12` -- CFGARM 2/n: the hub premise is FALSE. Four customers, four different blockers, and none of them is a CMD executor

```
CFGARM 2/n: the hub premise is FALSE. Four customers, four different blockers, and none of them is a CMD executor

This packet was commissioned on the shape "is there ONE missing CMD executor
behind I14, I30, I17's descriptors and I21's blocker 5?" There is not, and the
answer is a refusal with measurements rather than a build. Register 21 -> 21.

WHAT EACH OF THE FOUR ACTUALLY NEEDS, re-measured in this tree at this commit:

  I14 `proj_en_i`      an OWNER DECISION. No record, no executor, no wiring.
                       Unchanged; the one item a ruling could close today.
  I14 `pixel_error`    MEASURE.GOVERNOR, which is PARKED -- not uncomposed.
                       R223 item 4: its `cam0/1_thresh_q8_o` goes to
                       `zhao_geom_lodstate`, lodstate is inside R133's parked
                       FORGE.SHADOW subsystem, and R133 PRICED the register
                       cost in words and took it. The CMD.EXEC arms are the
                       SMALLEST part of this, and building them now would be
                       an uncashed cheque written against a standing ruling.
  I30                  DOES NOT EXIST. Closed and deleted 2026-09-19 under
                       owner ruling R45. I17 cited it as corroboration and
                       this packet's own brief inherited "three entries
                       agree" from that.
  I17 descriptors      NOT an executor gap: **the RECORD does not exist.**
                       `SetPlane` is ZERO hits in spec/commands.zidl.
                       SEARCHED and named: spec/*.zidl and spec/*.md for
                       SetPlane, SetSprite, DrawSprite, SetOverlay, DrawHud,
                       "2D plane", twod. The only ratified `plane` in the ABI
                       is SetPopulation's analytic COLLISION plane. Cartridge
                       page kinds run 0..13 and none is a 2D descriptor set,
                       so the R42/I33 published-page route is closed too.
                       spec/qformats.md 744-745 agrees from the other side:
                       the charter-16 2D plane mode is "listed by the charter,
                       OWNED BY NO SPEC YET". This is an ABI/page-kind owner
                       decision of R41/R42/R52's size.
  I21 blocker 5        its stated dependency on I14 HAD ALREADY EXPIRED.

THE EXPIRY IS THE FINDING, and it is the third time on one blocker.
`design/prod_manifest.yml` refused `zhao_view_projq88` because "the viewport
rect at projector cfg address 17 has no CMD producer". cfg 16 and 17 HAVE a
CMD producer: `zhao_cmd_exec` parses `SetView.viewport_id`, indexes
video_rules 3.2 and writes the rect as steps 20/21 of the view walk, landed
2026-09-20 (projbound), with I14's own bullet reading CLOSED since that day.

MEASURED, NOT READ OFF THE RTL, because a comment is what misled this blocker
twice already. `test_cmd_exec_directed` BUILT AND RAN at this commit:
**762 checks passed, RC 0, no `%Fatal`.** Cases 32-35 difference all four
rectangles against the ORACLE `zref::render::viewports_of()`; case 33 FIRES
`viewport_range_refused_o` by legal stimulus and asserts the camera still
lands (16 matrix words, views_written_o 1) while the rectangle is refused;
cases 34/35 are its negative controls and prove the counter reads the MODE and
not merely the id. `zhao_project_core.sv` 627/633/636 consumes addresses 16
and 17, so the rect has a real reader as well as a real writer.

AND THE ROT WAS LIVE IN PRODUCTION RTL. `zhao_cmd_exec.sv`'s EX_CFG preamble
justified its handshake with "the host port ... owns cfg addresses 16 and 17
(the viewport rect) THAT NO RATIFIED COMMAND CARRIES", sixty lines above the
arms that carry it and directly contradicting the comment beside them. The
manifest row refused a composition in exactly those words and I21 then
inherited the refusal from the manifest. A caution invented in a comment and
quoted by its neighbours is indistinguishable from a ruling -- which is
VIEWMASK's finding about the per-player tag, in a second block, one day later.
Corrected in place rather than deleted, with the handshake left alone: two
writers still exist and removing it would remove function. Only the reason was
wrong.

WHAT THIS CHANGES ABOUT SCHEDULING, stated because "nearly clear" reads as
"nearly worth doing": MEASURE.GOVERNOR's INPUT side is now entirely reachable
-- frame_i, starved0/1_i, proj0/1_i (projq88 <- projscale, whose vw is cfg 17)
and two ratified CMD.EXEC arms are all engineering. Its OUTPUT side is parked
by ruling. So the input work is correct and must NOT be done until R133/R199
move, or it produces five producers with no consumer on purpose.

I13 re-measured as I14's sibling and UNCHANGED: `u_geom_clip.tri_valid_i` is
`cl_in_valid` = `mw_t_valid && !cl_in_refuse_c`, still one producer chain
through `u_material_window`'s gate, still no merge. The two entries share a
port-group prefix and a subsystem and nothing else.

`zhao_view_projscale`'s deferral now cites a ruling, which it did not before;
by `uncashed_cheques.py`'s own rule a deferral citing none is an open question,
and that row read as an oversight for a month.

No RTL behaviour changed. No tie-off created; the audit holds at 0 SILENT.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
```

## `926df70f` -- CFGARM 3/n: I17's "one 8-bit port" tie-off is DONE, and the grep for it lands on the stale sentence

```
CFGARM 3/n: I17's "one 8-bit port" tie-off is DONE, and the grep for it lands on the stale sentence

Checked at the RTL rather than assumed from the head of the entry.
`rp_fb_tag_unused` and `rp_fb_addr_unused` are GONE from
`zhao_shell_top_v2.sv` (its own line 1131 says so), `fb_tag_o` connects to
`rpx_tag`, and the whole `gth_*` group leaves the shell -- the seven ports
whose absence from the paired-diff mutant broke every lane's configure at
aea45c4a (commit f056e586).

The consumer arrived FIRST (POST.GATHER composed, R218 under R195), so the
dangle that paragraph refused never had to happen -- which is the opposite of
the trade it was written to decline, and worth knowing before anyone quotes
R75 off it again.

Marked at the REFUSAL rather than only at the closure, because I17 records the
closure ~100 lines up at (c) and a reader who greps `rp_fb_tag_unused` -- the
literal string the refusal offers as its evidence -- lands on the stale
sentence and not on the fix. That is the same defect this packet's other two
commits are about, in a third place: the correction existed and was not
reachable from the citation.

Register 21 -> 21, tie-off audit 0 SILENT, quartus17 syntax clean.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
```

## `e1b4cbd7` -- CFGARM 4/n: R224's fifth customer measured. It makes the refusal five-for-five, not the hypothesis four-for-four

```
CFGARM 4/n: R224's fifth customer measured. It makes the refusal five-for-five, not the hypothesis four-for-four

R224 re-docketed I20's per-draw fragment constants onto this entry mid-packet,
on the reasoning that `tri_continuation_tail_i` IS zref's per-triangle
constant group field for field (24/8/8/8) and that "what has no producer is
the per-draw constant delivery path -- the same missing executor I14 and I30
describe". The field correspondence is real and is not disputed. **The
conclusion does not follow, and the two halves fail it for two DIFFERENT
reasons.**

`tri_fragment_state_i` -- ITS DELIVERY PATH IS BUILT, RATIFIED AND LIVE.
Owner ruling R28 ratified the word (`zref_raster_state.hpp`): [1:0] cull_mode
from `DrawForm.flags[3:2]`, [31:2] the material's half from
`MaterialRecord.raster_state[31:2]` "carried unchanged". CMD.EXEC lowers
DrawForm whole; GEOM.DRAWJOB composes the word; it rides the 72-bit draw-state
sideband (`GEOM_SIDE_W`, zref::drawjob's packing, R28/R29) through MESHFETCH,
ASSETFETCH and ASSEMBLE, and entry I39 CLOSED that carriage on 2026-09-20.
`zhao_material_resolve.sv:279/337/561` publishes the material half as
`rsp_raster_state_o` off the record's bytes 20-23, and it IS COMPOSED. Nothing
is missing on it anywhere.

`tri_continuation_tail_i` -- NOT a delivery gap: it is ABSENT DATA. SEARCHED
spec/*.zidl and spec/*.md for effect_tag, stencil_reference, vertex_rgb,
vertex_alpha, sten_ref, sten_mask: **ZERO hits** (the single match,
`cloud_vertex_alpha`, is a zref sky function). What holds the 24/8/8/8 layout
is `zref` -- the reference ORACLE -- which under R73's distinction makes these
DERIVED, not ABI fields. That supports R224's "no new ABI bits" and it moves
the question: derived FROM WHAT ratified input? For the viewport rect the
answer was `SetView.viewport_id`, a real ABI field. Here no ratified record
carries a vertex colour, an alpha, an effect tag or a stencil reference, so
there is nothing to derive from.

RECOMMENDATION, docketed rather than taken, because R224 asks for it to be
decided once: the ratified container with room is the MATERIAL_SET page
(kind 11), whose record already reserves bytes 24-31 (OFF_RSV0/OFF_RSV1, 64
bits against the tail's 48) and whose delivery is composed end to end. That is
R42/SPECIES_TABLE's pattern -- owner-authored DATA in a page, not a new
command. **It does not cover the whole tail:** effect_tag and
stencil_reference are material/primitive state and fit; vertex_rgb and
vertex_alpha are contested, because R11 makes base_rgb the VERTEX's colour and
not the material's, and putting them in a material record contradicts R11.
That is I13's and I20's open art question. **NO FIELD ALLOCATED HERE.**

AND THE FLAGGED QUESTION IS SETTLED IN THE LANE'S FAVOUR, NOT THE
COORDINATOR'S. TAGPROD's "`raster_state[31:2]` has no v1 consumer" is CORRECT,
and it is owner ruling R28's own sentence quoted in `zref_drawjob.hpp`: *"no
bit of it has a ratified consumer in v1, so a v1 material writes 0 there"*,
with the named constant `kV1MaterialRaster` and R48's ALPHA_C as the stated
precedent. The counter-citation is ABOUT A DIFFERENT WORD -- which is the
first of the two readings R224 offered, and it is the right one. Two 32-bit
words, two zref headers, both called "state":

  zref_raster_state.hpp   R28's DRAW-state sideband: [1:0] cull_mode,
                          [31:2] material half -> u_geom_assemble.m_raster_state_i
  zref_fragment.hpp       FragmentPipeline::State, the FRAGMENT word:
                          [0] z_test_en ... [31:24] sten_mask, matching
                          zhao_raster_fragment.sv 228-235 and 400-406 bit for
                          bit, all 32 consumed -> tri_fragment_state_i

So no correction is owed to that lane and no allocation is implied by it.

TWO CORRECTIONS TO R224's OWN CORRECTIONS, both measured at aea45c4a:
  * `// REAL:` in the core is **174**, not 173 and not 163. `// TIE:` is 0.
    Measured at the base commit, before any edit of mine, by a literal grep.
  * `-GlowTag` **does not exist in this tree**. `run_console_core_smoke.ps1`
    declares Mutant, UntexMutant, NoTableLoad, BadDescriptor, BadVertex,
    NoEchoArm, BadTraceArm, SkipVerilate and LintOnly, and no GlowTag.
    TAGPROD's branch has not reached this base, so there are SEVEN forms here,
    not eight, and the eighth cannot be run until the coordinator merges.

Register 21 -> 21, tie-off audit 0 SILENT, inventory OK, wrapper parity exact.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
```

## `e0ab6551` -- CFGARM 5/n: 223 citations say "owner ruling" for a coordinator-provisional one, and the owner's file has already struck this twice as a slip

```
CFGARM 5/n: 223 citations say "owner ruling" for a coordinator-provisional one, and the owner's file has already struck this twice as a slip

Found by checking my OWN citation. Commit 4/n called R28 an owner ruling. It
is not: the decision table's R28 row reads "**(provisional, coordinator)**
Ratify a `raster_state u32` layout in the ABI", and `zref_raster_state.hpp`
said BOTH -- "owner ruling R28" on line 1 and "(provisional, coordinator,
2026-09-19)" on line 2, contradicting itself in consecutive lines with the
wrong half first. I had inherited the wrong half, as the file invites.

MEASURED, with the instrument's positive control stated, because the first
run of the sweep returned ZERO and that zero was a bug -- a nested `-match`
clobbered `$Matches` before the id was captured. Reporting less than the
truth, in the flattering direction, exactly as CLAUDE.md says a broken
instrument always does. The corrected sweep sees 81 coordinator ids and
correctly excludes R1, which IS an owner ruling.

  reports/OWNER-RULINGS-20260919-EVENING.md's decision table:
    81 of 91 rows marked **(provisional, coordinator)**
    SEVEN rows in the whole table say "(owner, explicit)": R1..R7
  Sites saying "owner ruling R<n>" where R<n> is one of those 81:
    223 across fpga/, reference/ and design/
     62 in zhao_console_core.sv, over 26 distinct rulings
     27 in design/prod_manifest.yml, 16 in zhao_console_board.sv,
     10 in design/console_inventory.yml, 7 in design/blocks.yml,
     the rest through RTL and the reference oracle
  Of this file's 26, TWENTY-THREE have no prose section anywhere in reports/,
  so the coordinator-provisional table row is the only record and no later
  owner upgrade is being missed by the count.

NOT A NEW CLASS OF ERROR -- A KNOWN ONE AT SCALE. The owner's own file has
struck it twice, on R65 and again on R133: "**R133's D-FORGESHADOW-B is the
COORDINATOR's** ... Attributing a coordinator ruling to the owner is the same
mis-attribution this file struck R65 for, **and here it is doing real work**."
Both strikes treated it as one lane's slip. It is 223 citations.

WHY IT DOES REAL WORK. A packet reading "owner ruling R28 says every v1
material writes 0 there" believes it faces a frozen owner decision and stops.
A packet reading "provisional, coordinator" knows it can be re-asked for the
cost of asking. Same fact, completely different cost of moving it -- and the
core prints the first version 62 times. This campaign's standing complaint is
that blockers outlive their evidence; this is the same disease in the
ATTRIBUTION rather than the content, and it is larger.

NOTHING IS RE-OPENED. A coordinator's provisional ruling binds a packet
exactly as before: R133 and R199 still park FORGE, R223 still closes it to
packets, and this record licenses nobody to reach past one. What changes is
only WHO CAN LIFT IT -- information every escalation needs and none has had.
The repair is mechanical and touches hundreds of comments across files three
live packets are editing, so it is NOT done here; it is written down so it can
be scheduled once.

Recorded in the tie-off block's PRE-ENTRY prose, which attaches to no entry by
the placement note's own rule, because a record is not a gap. Register 21 ->
21, unchanged, and the KIND column of all nine entries is unchanged.

ALSO, on the coordinator's paired-diff message: I HIT THAT FAILURE AND FIXED
IT FOUR COMMITS AGO (f056e586), before the note arrived. **Our two fixes
overlap and will conflict; mine is a superset and here is the difference.**
Both regenerate the mutant and both fix the tool's wrong remedy line. Mine
ALSO makes a bare `--check` check BOTH generated files, so the form that
PACKET-PROTOCOL.md and every brief already runs cannot go blind again --
rather than relying on each packet remembering `--check --mutant`. Recommend
taking this branch's `tools/design/gen_shell_paired_diff.py` whole at merge;
the regenerated mutant is deterministic generator output from the same shells
and should be byte-identical to c4030357's.

AND SHEETSEAM'S FINDING SHARPENS, VERIFIED HERE. It is not only that a gate
which cannot be INSTALLED is not evidence. `add_test(NAME
packet_h_paired_diff_mutant_fresh` is at tests/CMakeLists.txt:8356; the
`verilate()` that aborts on the stale mutant is at :8346, ten lines above.
**So the detector's installation is conditional on the absence of the fault it
detects -- it is disabled exactly and only when it would fire.** With the
mutant fresh, the configure completes and it installs: it is Test #638 in my
tree and `ctest -R packet_h_paired_diff` is 2/2 PASS, RC 0. The general form,
worth a sweep: any ctest registered AFTER a `verilate()` of the thing it
checks is blind to that thing's breakage.

Both detectors for this fault were dark at once, for the same structural
reason -- each depended on something the fault broke. The Python gate is the
one that has to survive it, because it runs OUTSIDE CMake and therefore still
works in the state where you need it. That is the argument for the structural
half of f056e586.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
```
