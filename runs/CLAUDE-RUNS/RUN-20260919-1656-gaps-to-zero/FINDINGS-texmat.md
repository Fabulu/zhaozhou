# FINDINGS — texture/material/measure packet (1st pass)

Transcribed by the coordinator from the packet's final report (the harness blocks subagents
from writing report files). Register at d3491ce5: **56**.

## Landed
* `3e4748a9` MATERIAL.RESOLVE mutant copy refreshed (production had changed only a comment).
  The baseline's `check_prod_manifest` RED was NOT a tree defect. It appears only where Verilator
  cannot load (the coordinator's baseline worktree lacked a winlibs folder); at HEAD it reads OK.
  **Instrument defect:** when elaboration fails, the gate still prints parameter-blind "the fit
  would die" errors instead of saying elaboration failed. That is how a stale RED got inherited.
* `b4336db3` R4: the HPS arbiter takes N clients; the 2-client version is now a thin wrapper, so
  every existing site is unchanged. Starvation law kept: the lower index has strict priority, and
  every waiting client from index 1 up is counted. Proved with 69 checks at N=2 and 111 at N=3
  against the real bridge; client 2's wait counter was made to fire. Also keys the smoke bench's
  build folder per checkout: packets were sharing one `%TEMP%` folder and breaking each other's links.
* `902949ea` MEM.SHARE takes N requesters, round-robin bounded at N-1; 54 checks at N=3, and the
  35-check adapter test passes. The adapter stays at 2 ports until the resolver has a request
  producer, since a third port driven by nothing would be a new tie-off.
* `d3491ce5` R9: `zhao_texture_v3own` counts TMU samples actually delivered to a fragment (a
  commit that fails the generation check is not counted), exported as island
  `cnt_texture_samples_o`. The packet_b test asserts it exactly: +0 for a no-sample fragment,
  +1 NEAR, +1 CLUT. TEXTURE.TMU → `zhao_texture_v3own`; the two old TMU modules are marked superseded.

## Refused
* **The island still samples nothing.** Every raster triangle enters at the I24 edge, and GEOM.REPLAY
  (I11) is the missing carrier of meshlet material. The material seam follows replay.
* **MATERIAL.RESOLVE would hang on a denied fetch.** Its memory port has no error input, and the
  oracle has no "fetch denied" status. Both are needed before it goes behind MEM.GUARD.
* **MEM.UPLOAD:** no ratified command carries an upload request (→ R17).
* **MEASURE.TOKENS / GOVERNOR:** the budget has two sources and two units (→ R18).
* **I18 / I19, I41:** I41 waits on the asset-pool layout (I36).
* **Counter catalog:** TEXTURE.MOSAIC and TWOD.SPRITE still list `texture_samples` (→ R19).

## False claim #16
The TMU ledger row said no v3 module emits a samples counter; `zhao_texture_mosaic_v2` does (it
counts mosaic picks). Corrected in the row.

## Pre-existing red
The packet C/D/E/F/G registration tests pin stale hashes of `zhao_shell_top.sv` and
`zhao_prod_top.sv`. Left alone.

## Trap
In PowerShell the comma binds tighter than `+`, so `@("a", $x + "b")` is three elements. It
dropped lines from two mutant copies; caught in the diff before commit.
