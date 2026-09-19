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
   - ~~no CLUT8/RGB565 sampler~~ — **imprecise, corrected below**
   - ~~RESOLVE internal to `zhao_geom_bin_pipe_v2`~~ — **FALSE, corrected below**
   - ~~no placement/command owner~~ — **the command half was ENCLOSED, not
     absent; see below**
   - nothing projects a particle — blocks `part_expand`, `part_soft`, `part_ladder`

## Corrections to this document, 2026-09-19 (same day)

Three of the five root causes above were wrong or imprecise, which is a bad
enough rate that the list itself was the problem: it was written fast, from
memory, in a document whose whole subject is other people asserting absences
they had not checked. Recorded here rather than quietly edited.

**"RESOLVE internal to `zhao_geom_bin_pipe_v2`" is FALSE.** RESOLVE's stream is
NINE PORTS on that module — it is exported, not enclosed. What is not
re-exported is one level up, in the shell. And neither is the real obstacle,
which is **ordering**: RESOLVE emits tile-major and the post path wants
full-frame raster. That is a buffering and scheduling question, not a wiring
one, and it will not be closed by adding ports.

**"no CLUT8/RGB565 sampler" is imprecise in the direction that sends someone to
build a duplicate.** `zhao_texture_tmu_pipe` decodes CLUT8 against a resident
palette and `zhao_texture_palette_res_v2` is that palette. Entry I17 in the
core's header states the actual obstacle correctly and this summary flattened
it: the TMU is FRAGMENT-SHAPED and sits on the raster path, so what is missing
is a block of the 2D request shape, which may well be an adapter over the
existing decode rather than a new sampler. I17 is the authority; this line was
not.

**The command half of "no placement/command owner" was ENCLOSED, not absent —
and it is now open.** CMD.DMA's packet stream was real and already flowing, and
it was body wires inside `zhao_shell_top_v2` with no way out, so five register
entries (I7, I14, I30, I33, the FORGE.PRIM jobs) each recorded it as a missing
owner. The shell now re-exports it and CMD.DECODER is composed. What remains
genuinely absent is the command EXECUTOR — `spec/commands.zidl:382` says so in
as many words, "NOTHING TURNS A COMMAND INTO A FRAME yet" — because the decoder
produces record headers and a verdict, and `SetView`'s `mat4fx` and
`SurfaceStamp`'s transform are payload that no header carries.

**The pattern across all four corrections is one habit.** Every wrong entry was
a claim about what does not exist, made without searching, and every one of them
pointed work at the wrong place. Rule 4 below was written in this document to
stop exactly this and then this document's own list broke it six lines earlier.
A refusal must name what it searched — including when the refusal is mine.
4. **A refusal must name what it searched.** "X does not exist" is a claim, and
   this repo has now produced three of them that were false — the phantom
   register that missed `FORGE.SHADOW`, "no owner exists" for a table that had
   been built the same day, and this one.

## What does NOT change

The register stays the scoreboard, and it stays computed. **Fit at completion
only.** And nothing gets smaller by removing function — the rule that makes the
eventual number mean anything.
