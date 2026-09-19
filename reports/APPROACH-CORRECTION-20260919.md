# Rethinking the approach — 85 → 79 in a day is too slow

Owner, 2026-09-19: *"is this bringing us closer to the goal of actually
connecting the console?"* and *"rethink your approach and maybe actually work on
the goal."*

Both fair. This records what was wrong and what changes.

## The rate, stated plainly

| | start of Phase 1 | now |
|---|---:|---:|
| mandatory gaps | 85 | 79 |
| disconnected | 57 | 46 |
| tie-offs | 20 | 27 |
| unbuilt | 8 | 6 |

**Six net gaps.** Eleven modules connected, two built, three tie-offs genuinely
closed — against several hours and well over a million tokens. One agent spent
**146,700 tokens checking line endings**.

## What was actually wrong, and it is mine

**1. The acceptance ceremony was sized for defect repair and applied to wiring.**
Every packet brief demanded: lint both ways with a positive control fired inside
the agent's own edit, a bench fire-test with exact text, mutant refresh,
register before/after, smoke evidence, `format_check`, `check_quartus17_syntax`.
That is right for repairing a silent data-loss defect. For connecting a module
that already works and already has its own tests, it is most of the cost.

**2. Composing one module at a time rediscovers the same blocker repeatedly.**
The refusals cluster: five root causes hold roughly 25 of the 46. Each packet
spends ~200k tokens re-deriving one of them.

**3. And at least one of those blockers is FALSE.** Which is the finding that
prompted this document.

## The false blocker

The geometry packet refused `meshfetch`, `assetfetch`, `assemble` and `parambuf`
with: *"there is no behavioural SDRAM model in this tree."*

**`sim/models/zhao_sdram_model.sv` exists** — 219 lines, cycle-true, honouring
the frozen sim profile, checking every timing law with sticky per-kind error
outputs. `zhao_console_board.sv` already references it. The packet asserted
absence without searching `sim/`, and four modules were refused on it.

**And the refusal is wrong a second time over, which matters more:** those
fetchers do not need an SDRAM model at all. `TERRAIN.PAGE_POOL` at `0x0400_0000`
and the render asset pool at `0x06A0_0000` are the same flat address space on
the same MEM.GUARD path — and the terrain paging path **already works in the
smoke bench**, retiring whole pages, because the core exposes `terr_hps_*`
boundary ports that the bench drives.

The completion plan permits exactly this: *"The simulation harness supplies
external clocks, input events, memory behavior and host packets."* What it
forbids is the harness supplying *missing lighting, FIELD results, particle
updates, prepared triangles or fake material records* — none of which is memory.

**So the geometry fetch path is composable now, by the pattern terrain already
proved.** It was refused on a blocker that does not exist.

## What changes

1. **Stop delegating composition.** Wiring ports is faster done directly than
   briefed, reviewed and reconciled. Agents stay for work with genuine
   uncertainty — a defect hunt, a contract conflict, a new block.
2. **Cut the ceremony for composition packets** to: waived lint silent, smoke
   RC 0, register before/after. The heavy evidence discipline stays for defect
   repairs and new RTL, where it has repeatedly earned its cost — it found the
   child loss, the 700,109 counter, the harness static-lifetime bug and three
   gameable holes in the register itself.
3. **Attack root causes, not modules.** Remaining:
   - ~~no behavioural SDRAM model~~ — **FALSE, see above**
   - no CLUT8/RGB565 sampler — blocks `twod_plane`, `twod_sprite`, `post_gather`
   - RESOLVE internal to `zhao_geom_bin_pipe_v2` — blocks the whole post path
   - no placement/command owner — blocks terrain `patch`, `compcache`, `visible`
   - nothing projects a particle — blocks `part_expand`, `part_soft`, `part_ladder`
4. **A refusal must name what it searched.** "X does not exist" is a claim, and
   this repo has now produced three of them that were false — the phantom
   register that missed `FORGE.SHADOW`, "no owner exists" for a table that had
   been built the same day, and this one.

## What does NOT change

The register stays the scoreboard, and it stays computed. **Fit at completion
only.** And nothing gets smaller by removing function — the rule that makes the
eventual number mean anything.
