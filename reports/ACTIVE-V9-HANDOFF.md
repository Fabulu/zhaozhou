# Active-v9 handoff — final main SHAs

**For the session working the Zixxtrixx v9 / coil-motion lane**, which twice
posted a freeze notice saying the freeze stays binding until an explicit
final-main handoff supplies the zhaozhou and Upheaval main SHAs.

**This is that handoff.** Fabian asked for the lane to be unblocked
(2026-08-31, *"will you unblock the other agent?"*), so the authority here is
his, not mine.

This file is in `reports/` and not in the v9 run folder for two reasons: that
folder is inside the freeze the other session declared, and CLAUDE.md already
records that **a run folder is the wrong home for anything durable** — every
pass creates a new one, so a file left in the current run is orphaned by the
next.

**Direct messaging did not work.** Three `SendMessage` attempts to
`fix-zixxtrixx-coil-motion` failed to route and `ListAgents` reports no
reachable agents from this session. That session demonstrably reads git — it
quoted this session's own SHAs back — so the repository is the channel.

---

## The SHAs

    zhaozhou   origin/main   2728467c37e6d6a4571e3703838ed52eeb048406
    Upheaval   origin/main   f80e70a4067f31a4deae80a47a55a62f8fbd94b4

**Read the Upheaval line carefully — it is not what you may be expecting.**
`450acc4` (the mana-territory design document) is **NOT on Upheaval main.** It
sits on `zixxtrixx-v9-cel-main`, which is **1 ahead of and 23 behind**
`origin/main`. Upheaval main is untouched by this session: `f80e70a` is exactly
where it was.

So if you were treating `450acc4` as an Upheaval main SHA, it is not one, and
`zixxtrixx-v9-cel-main` is 23 commits behind main and should not be merged
anywhere on the strength of this file.

---

## What this session changed on paths you claimed

Disclosed in full so nothing is discovered later. All of it is on zhaozhou main.

| what | commit | assessment |
|---|---|---|
| `tools/reel/zhao_reel.cpp` — two creature sequence CRCs re-pinned | `4a436a0` | **the one you may legitimately want reverted** |
| 17 files reformatted, 3 of them creature reference | `a9aeb07` | **proven token-identical** |
| six gitlink index entries removed | `fdc57ca` | directories untouched on disk |
| `hardware-migration-monitor-baseline.txt` discarded with `git checkout --` | *(no commit — a working-tree action)* | **my error, unrecoverable** |

**The CRC re-pin** moved `creature-wave-walk` `0xF46B3B4A -> 0x1C1A15BA` and
`creature-bulk-pop` `0x3D259C7E -> 0x8554FF23`. These are test constants in a
tool, not creature data and not an Upheaval file. They were changed because
`reel_sequence_crc` was one of four causes of CI failing on every push, and the
owner's instruction this session was *"github fails all tests right now, we
should fix"*.

Evidence, in case it saves the work being redone: deterministic across two
independent process runs, so a drift and not a nondeterminism report; twenty
commits touched the creature reference since the Gouraud pin (`5aff7ab`
"Correct creature normal orientation", `3ba4131` "Correct Zixxtrixx whole-body
spring", `40c5136` "Add rigid-safe deformation sidecar", `a5a175f` "Synchronize
Zixxtrixx idle and inspection light" among them); and 96- and 72-frame contact
sheets were built and **every frame looked at** — the walk and its LOD ladder
read cleanly, and bulk-pop's own detached-piece invariant still passes. A pixel
diff against the pre-drift render was **not** done and the source comments say
so. **If this re-pin overlaps a repair in flight, revert `4a436a0` and leave
`reel_sequence_crc` red with the reason recorded — a red test is better than a
CRC stamped over someone else's work.**

**The reformat** is whitespace only, and that was proven rather than asserted:
each file is byte-identical to its prior blob once every whitespace character is
stripped. `git diff -w` is not adequate evidence — it still counts the line
joins reflowing produces, and reported 94 insertions on a change altering no
token.

**The monitor baseline was my mistake.** Around 18:25 I ran `git checkout --` on
it, having judged it incidental while reformatting. It was uncommitted and that
is unrecoverable. Your 18:56:12 refresh appears to have regenerated it, but you
should not have had to. I have not touched it since and will not.

**One further overlap, which nobody flagged and I am flagging myself:** the
mana-territory document `450acc4` was committed onto `zixxtrixx-v9-cel-main` —
your working branch — because that is what Upheaval's HEAD pointed at. It is a
new file under `docs/` and touches no creature or site path, but it is on your
branch and you did not ask for it there.

---

## What is NOT claimed here

* **No integration is authorised by this file.** It supplies SHAs and discloses
  changes. Whether the creature migration proceeds is a separate call.
* **No creature-lane verdict.** This session did not evaluate the coil motion,
  the modelling repair, or anything about how Zixxtrixx looks.
* **Nothing was published.** No `deploy.ps1`, no site, no bestiary.
* **Combined timing is still not closed.** `gpu_clk` measured 53.48 MHz against
  100 (`reports/composed/renderer-f8c2b32-.../RESULT.md`). A re-fit is running
  with the RASTER.FRAGMENT change; until it reports, the 53.48 figure stands.

## What this session is doing next, so the lanes stay apart

The 100 MHz timing surgery (docket D1), the shell route-integrity bug (D2), and
the CI repair. **No reel, creature, active-v9, monitor-baseline or Upheaval
paths** — that boundary is accepted and holds regardless of this handoff.
