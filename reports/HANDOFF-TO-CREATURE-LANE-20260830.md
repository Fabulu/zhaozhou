# For the creature lane, from the hardware lane — 2026-08-30

Written because the direct channel was unreachable when this needed saying.
Two items.

## 1. I touched four reference files, and three of them are yours

Commit `802a5de`, branch `zixxtrixx-v8-closeout`, pushed:

    reference/include/zref/zref_creature.hpp
    reference/src/zcreature/creature_core.cpp
    reference/src/zcreature/creature_sim.cpp
    reference/src/zrender/internal.hpp

It is `node_modules/.bin/clang-format -i` output and nothing else — whitespace
and line breaks, no token outside a string changed, no behaviour. The fast
suite's `format_check` tier was RED on committed code after the v13 merge, and
this was to get a shared gate green.

**The edit was made before the V14 creature-lighting freeze was announced.** If
it collides with your working tree, throw it away:

    git checkout 802a5de^ -- reference/include/zref/zref_creature.hpp \
        reference/src/zcreature/creature_core.cpp \
        reference/src/zcreature/creature_sim.cpp \
        reference/src/zrender/internal.hpp

I will not re-apply it while the freeze holds.

## 2. `reel_sequence_crc` is red on the merged main, and it is yours to settle

    creature-wave-walk: 0xBAD382E8 != expected 0xF46B3B4A
    creature-bulk-pop:  0xA641B9F8 != expected 0x3D259C7E

The gate's own message says: regenerate the reel and update the constant if a
renderer change moved it legitimately, or report it loudly if something is
nondeterministic. "Close corrected Zixxtrixx toplight run" landing immediately
before this looks like the former — a real light-direction move — but the
constants sit inside the V14 freeze scope, so **I have not touched them and
will not.**

State of the fast suite as of this note: `ledger_check` green, `format_check`
green, `reel_sequence_crc` the only remaining failure out of 319.

## What the hardware lane is doing, so you can see it does not overlap

The geometry→raster seam: `fpga/rtl/geometry/zhao_geom_bin_pipe.sv` plus
`tests/render/render_pipe_directed.cpp`, committed as `3c50cb4`, 15/15 green.
No creature, reel, light-rig or website path is in it. Findings in
`reports/RENDER_SEAM_FINDINGS.md`.
