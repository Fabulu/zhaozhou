# What actually couples the two repositories — measured, 2026-08-29

Measured against zhaozhou `f6626a0`. Numbers, not impressions, because the
report's premise deserves checking rather than repeating.

## The headline: the reverse dependency is ONE test and ONE include

    ctest reel_sequence_crc          (tests/CMakeLists.txt:168, LABELS "fast;nightly")
      -> zhao-reel                   (tools/CMakeLists.txt:12)
        -> #include "zixxtrixx.h"    (tools/reel/zhao_reel.cpp:1407)

That is the whole of it. Everything else called "coupling" hangs off that one
edge.

## What is already clean, and this is the good news

**`reference/` has no dependency on the creature at all.** Six mentions of
"zixx" across `zref_creature.hpp`, `creature_sim.cpp` and `creature_core.cpp`,
and **all six are comments** — each citing the creature that motivated a rule:

    "the first Zixxtrixx opened a 61 mm hole (5.3 px on a 19 px body)"
    "(Zixxtrixx's eye) sat near U = 0. The fix is a per-ring DUPLICATE"

That is a generic contract with its provenance written down, which is what the
report asks the reference to be. It does not need migrating.

**`tests/CMakeLists.txt` names no creature content** except the single
`zhao-reel --check` line above. The console's verification is already
independent of the creature.

So "zhaozhou builds and proves itself with no game checkout" is **one test
away**, not a rearchitecture.

## The production content, sized

    tools/reel/zixxtrixx_page.h    18,149 lines   GENERATED, and tracked
    tools/reel/zixxtrixx.h          5,075 lines   the model
    tools/pack/mkcreaturepage.py    1,403 lines   the generator
    tools/reel/zixx_*.cpp           1,047 lines   7 probes
    tools/reel/zhao_reel.cpp        4,490 lines   39 zixx subjects, MIXED
                                   -------------
                                   ~29,100 lines

`zhao_reel.cpp` is the only genuinely mixed file: 39 `zixxtrixx-*` subjects
alongside `terrain-wave`, `terrain-impact`, `terrain-scars`, `terrain-orbit`,
`terrain-breach`, `sky-sweep`, `star-boil`, `noctis-flare` and others that are
console content and stay.

**The single biggest item is generated output that is tracked.** 18,149 of the
29,100 lines are a texture page a script produces. Untracking it is not part of
the ownership question at all and would remove 62% of the bulk on its own.

## Three separate messes, and only one of them is frozen

**1. Untracked clutter — 98 entries, fixable now, blocked by nothing.**
Render output written into the repository root: 19 `zixxtrixx-*/` directories,
`atmo-sun-donor/`, `atmo-sun-thick/`, `blue-dwarf/`, `blue-giant/`,
`captures/failures/*`, `probe-salto.txt`, `bash.exe.stackdump`. None tracked.
This is tools defaulting their output to the working directory instead of
requiring one. The report already asks for an explicit output directory; the
same change fixes this.

**2. The ownership move — frozen until the v9 handoff.** One include, one test,
five files plus the mixed reel. Genuinely small once the generated page is
untracked.

**3. Lane collision — fixable now, and this is what is actually costing days.**

    31 of 34 mutation-sweep drivers build in a SHARED tree
      8 default to build-verify  (the tree an interactive session uses)
      5 default to build
     18 hardcode `build` with no knob at all
      3 have dedicated trees

That is not hypothetical. On 2026-08-29 two of those drivers collided with an
interactive session; one was killed mid-run and left a **mutant applied to
shipped RTL**. It was caught, and only because a guard exists — a guard which,
it turns out, **cannot pass in a clean clone** (it infers liveness from mtime,
and cloning resets every mtime, so every historical log looks live).

Layer 3 is the reason the two lanes keep stepping on each other. It is not
about repository ownership at all, and none of it is frozen.

## What I would fix, in this order

1. **Untrack the generated texture page and require an explicit output
   directory.** Removes 62% of the bulk and stops the root filling with render
   dirs. Unblocked — except `mkcreaturepage.py` itself is frozen, so the
   `.gitignore` half lands now and the generator change waits.
2. **Give every sweep driver its own build tree**, or make a driver refuse to
   start when another process is using its tree. Unblocked, mine, and it is the
   fault that has actually bitten.
3. **Fix the clean-clone guard.** Liveness must come from a live PID, not a
   timestamp that no copy preserves.
4. **The ownership move**, after the handoff, which by then is a small
   mechanical change rather than a 29,000-line one.
