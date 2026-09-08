# The RCP V3 swap recommendation rests on a mismatched comparison

2026-09-08. §14.5 asks to qualify RCP V3 at the island's profile in a separate
lane. Doing the reading first turns up that the comparison already on record is
not one.

## What was quoted

I recorded, and repeated, this:

> `zhao_raster_rcp24_v3` at TOKW=14: **1034 ALM / 3 DSP / 93.67 MHz** against
> svc's **1041 / 6 / 68.46**.

Smaller, half the DSP, 25 MHz faster. It reads as a decision that makes itself.

## What the two rows actually are

`topParameters` is recorded per row, and it settles it:

| row | topParameters | effective NCTX | effective TOKW | ALM | reg | DSP | **M10K** | Fmax |
|---|---|---|---|---|---|---|---|---|
| `zhao_raster_rcp24_svc` | *(none)* | **8** (default) | **8** (default) | 1041 | 1101 | 6 | **0** | 68.46 |
| `zhao_raster_rcp24_v3@tokw14` | `TOKW=14` | **16** (default) | 14 | 1034 | 1478 | 3 | **8** | 93.67 |

**Two parameters differ, in the direction that flatters V3 twice over**, and a
whole resource class was never mentioned:

* **NCTX 16 against 8.** V3 was fitted carrying *twice the contexts* and still
  came in 7 ALM smaller. That is not "V3 is smaller"; it is a different machine.
* **8 M10Ks against 0.** V3's ALM count is lower partly because ~1,800+ bits of
  state moved into block RAM. The M10K column never appeared in the sentence I
  wrote. On a device where the island already sits at 48 of 553, eight more is
  affordable — but it is a cost, and it was invisible in the comparison that
  drove the recommendation.
* Registers 1478 against 1101, which is +34% and also went unquoted.

This is the repository's own law about mismatched comparisons, the one written up
after a creature's tube was thickened 2× in the wrong direction: **compare like
with like, or do not compare.** I did not.

## The run that would have been like-for-like failed, and why

There is a row for exactly the right experiment:

```
zhao_raster_rcp24_v3@island-profile
    topParameters   'NCTX=8,TOKW=14'
    status          incomplete:failed:quartus_map.exe
    seconds         38.3
```

It failed in 38 seconds, and the commit that recorded it said the failure showed
"NCTX above 11 million — **NOT** the block rejecting NCTX=8", leaving block-vs-tool
open. It is the tool, and the mechanism is one line of
`tools/quartus/run_block_fit.ps1`:

```powershell
$kv = $tp -split '=', 2
$qsf += "set_parameter -name $($kv[0]) $($kv[1])"
```

`-TopParameters` is `[string[]]`, but the value arrived as **one quoted string**
`'NCTX=8,TOKW=14'` rather than a two-element array. Reproduced:

```
elements: 2
name  : NCTX
value : 8,TOKW=14
QSF line emitted: set_parameter -name NCTX 8,TOKW=14
```

So `NCTX` was set to the garbage `8,TOKW=14` — hence eleven million — and **TOKW
was never set at all**. The receipt itself preserves the evidence: it records
`'NCTX=8,TOKW=14'` with a comma, whereas a correctly-passed array joins to
`'NCTX=8 TOKW=14'` with a space.

The failure mode is the familiar one. The script accepted a malformed entry
silently, emitted a nonsense QSF line, and the error surfaced 38 seconds later
in a form that reads like the *block* rejecting the parameter. A guard that
rejects a `TopParameters` entry whose value contains `=` or `,` — naming the
array-versus-string mistake — turns a confusing 38-second failure into an
immediate one. **Not yet applied**: `run_block_fit.ps1` is the script currently
executing the `@d0fixed` island fit, and while PowerShell parses a `-File`
script once at launch, there is no reason to spend a 2.5-hour measurement to
prove it. It lands when that fit does.

## What §14.5 actually needs

1. Fix the `TopParameters` guard.
2. Re-run `@island-profile` at **NCTX=8, TOKW=14** — svc's context count, the
   island's token width — and compare that against `zhao_raster_rcp24_svc`
   re-fitted at **TOKW=14** as well, since the standing svc row is TOKW=8.
   Neither of the two rows on record is at the island's profile.
3. Report ALM, registers, DSP **and M10K** together. The DSP 6 → 3 halving is
   the one claim in the original sentence that survives both parameter
   mismatches unscathed, and it is a real one.

Until then the swap is **not recommended and not refused** — it is unmeasured.
Presenting it as a decision awaiting the owner overstates what is known.
