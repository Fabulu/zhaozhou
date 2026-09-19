# Phase 1 composition plan — 85 gaps to zero

Computed from `python tools/budget/completion_register.py`, not assembled by
hand. Re-run it; if this document and the tool disagree, **the tool is right**.

```
MANDATORY GAPS = 85
  20  tie-offs in zhao_console_core
  57  BUILT BUT NOT CONNECTED
   8  NOT BUILT AT ALL
   0  uncited excuse
   0  unresolvable
```

**Nothing is deferred.** Every remaining gap is work.

## The shape of the problem

Only **8 of 85** need building. The other 77 are wiring and tie-off closure.
That is the single most useful fact in this plan: Phase 1 is a **composition**
campaign, and its risk is not "can we write this block" but "does the seam
between two blocks that were written months apart actually meet".

Today already produced three examples of that seam failing:

* PART.UPDATE expects a collision velocity PART.COLLIDE never emits (ruled, I4);
* PART.STATE grew `capacity_full_o` and the core did not connect it — the tree
  would not elaborate (closed, I8);
* `zhao_terrain_patch` emits heights and **no surface normal**, which the
  collision test needs (open, I6).

Expect one of these per packet. They are the work, not an interruption to it.

## The 57, by subsystem

| packet | n | modules |
|---|--:|---|
| **P-TERRAIN** | **20** | cmd, mipfeed, pagestream, loadq, pageloader, writeback, patch, shade, compcache_front, island_dir, seq, visible, residency, mipgen, normalmap, normals, velocity, bake, lod, project |
| **P-GEOM** | **12** | meshfetch, assetfetch, vdecode, pose_decode, skin_norm, parambuf, project, clip, setup, assemble, light, depthquant |
| **P-TEXTURE** | 7 | tmu, aux, cache, combine, fragrob, material_combine_v2, mosaic |
| P-FORGE | 3 | prim, prim_eval, cliff |
| P-PART | 3 | ladder, expand, soft |
| P-2D | 2 | twod_plane, twod_sprite |
| P-FIELD | 2 | field_progcache, field_v2_core |
| P-SURFACE | 2 | surface_sheet, surface_stamp |
| P-MEASURE | 2 | measure_governor, measure_tokens |
| P-MISC | 4 | post_gather, debug_trace, video_slotmgr, cmd_decoder |

## Order, and the reasoning

**1. P-GEOM first, despite terrain being larger.** It unblocks the most
tie-offs: I10 (GEOM.SKIN's vertex/matrix inputs), I11 (GROUP_SEQ's replay
customer is GEOM.SETUP), I12 (PROJ_LANE's lookup customer), and it is the
prerequisite for I15's real fix — the compositor cannot sit *in* the render path
until the vertex front end feeds it. The shell's own source list records that
its geometry front end "elaborated nowhere and was struck", so this is a
restoration with a known-good target.

**2. P-TERRAIN second.** Largest, and I13/I21/I22 already opened its seam. Its
own internal ordering is residency → pagestream/loadq/pageloader → patch →
normals/normalmap → shade → bake/writeback, because each consumes the previous.
`terrain_project` is the one to watch: it carries a private `zhao_project_core`
at 33 DSP, and composing it naively re-duplicates the projector the shared
service was built to remove.

**3. P-TEXTURE, P-FORGE, P-SURFACE, P-2D** — feed established endpoints.

**4. P-FIELD** — and `field_v2_core` is FROZEN as a fallback, not the production
executor. FIELD v3 is mandatory and its production composition is a separate
question the completion plan raises; do not let a v2 row make it look closed.

**5. P-PART, P-MEASURE, P-MISC** — small, and mostly ingress/egress for blocks
already in the core.

## The 8 that need BUILDING

```
SYS.PLL        SYS.RESET        MEM.UPLOAD      INPUT.SNAC
GEOM.LOOM      GEOM.WARP        FORGE.SHADOW    POST.ECHO
```

**Three of these need their CONTRACT authored first** — GEOM.WARP, INPUT.SNAC
and POST.ECHO read "Deliberately unwritten" in all twelve sections, because they
were cut before anyone specified them. Writing RTL against an unwritten contract
is inventing the specification, which is what those headers exist to forbid.

**SYS.PLL and SYS.RESET are now writable** and were not until 2026-09-13:
`board_truth.json` gives FPGA_CLK1/2/3_50 at 50 MHz on pins V11/Y13/E11,
3.3-V LVTTL, hpsOsc1 at 25 MHz, buildTarget 5CSEBA6U23I7, and a reset contract
of "MiSTer HPS/framework RESET, synchronized release after core PLL lock".
What is NOT yet derivable and must be read rather than assumed: the exact
relationship between `gpu_clk` and `video_clk`. `zhao_pkg` gives `v_total` 262
for every mode and per-mode H totals of 480/416/608, which yields pixel rates of
7.5456 / 6.5395 / 9.5578 MHz at 60 Hz — but its "gpu cycles/frame" figures are
exactly 2x the pixel-clock count, and reconciling that with the 100 MHz gpu
target is a reading, not an arithmetic step. **Do not author the PLL contract
until that is settled from the source.**

## The rule that governs all of it

**FIT AT COMPLETION ONLY** (owner, 2026-09-19). No Quartus of any kind until
this register reads zero. The register is the progress instrument and it costs
seconds; a fit of an incomplete console measures a machine nobody intends to
build, and this session already spent ~3.5 hours proving that once.

And from Phase 2's rules, which apply retroactively to how Phase 1 is done:
**never make the number smaller by removing function.** A tie-off deleted
without being closed, or a module dropped from the closure to quieten a warning,
is the one failure this whole instrument exists to catch.
