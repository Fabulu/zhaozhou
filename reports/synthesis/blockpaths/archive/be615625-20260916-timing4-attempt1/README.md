# `@g8a-timing4` attempt 1 — `be615625`, 2026-09-16

A COMPLETE, TRUSTWORTHY MEASUREMENT WITH NO RECEIPT. Both halves of that
sentence matter, and the reason for the second is an operator error, not
anything wrong with the fit.

## What it measured

Source commit `be615625f027679f824537fdabe03c760c5dd7e2`, `rtlCleanAtHead:
true`, `treeCleanAtHead: true`, seed 1, 48 sources, digest
`e7947623b3e38b7729496d7533b94c7f84dc4e84861f3dbd847ffc05975ff205`,
`physical-top-ports`, 846.8 s. The row is in `reports/synthesis/zhao_block_fit.json`
as `zhao_raster_texture_v3_fit_top@g8a-timing4`.

| | Timing3 `3bf599d5` | **Timing4 attempt 1** | delta |
|---|---:|---:|---:|
| ALMs | 13,195 | **12,940** | −255 |
| Fmax | 90.96 MHz | **94.46 MHz** | **+3.50** |
| setup WNS | −0.994 ns | **−0.587 ns** | +0.407 |
| setup TNS | −131.275 ns | **−0.721 ns** | **+130.554** |
| hold | +0.242 / 0 | +0.237 / 0 | — |
| DSP | 30 | 30 | 0 |
| RAM blocks | 71 | 71 | 0 |
| memory bits | 92,964 | 92,964 | 0 |
| registers | 22,496 | 22,735 | +239 |

`status: failed:structure`, one rule violation: *Fmax 94.46 MHz < required
100 MHz*. Per the repo law that is not a failed measurement — the fit completed
and the budget rules rejected the number.

## Why there is no receipt

The runner threw **after** the fit, on its own post-check:

> The generated G8A fit manifest changed after the pre-fit snapshot.

That check was right and the fault was mine. The fit snapshots its 48 sources so
the live tree cannot reach it, and I read that as licence to keep working inside
its declared closure. It does protect the MEASUREMENT — the numbers above are
from the snapshot and are sound — but the D2 work I did during the run edited
`zhao_raster_tile_pipe_v2.sv` and regenerated the G8A wrapper and manifest, so
the artifact the runner compares against at the end no longer matched the one it
started from, and it refused to stamp a receipt tying this row to a manifest.

Refusing was correct. A receipt whose manifest describes a different set of
sources than the fit consumed is exactly the worthless-but-reassuring row the
`rtlCleanAtHead` law was written about.

**The rule this cost:** a snapshot protects the fit from the tree. It does not
protect the RECEIPT from the tree. Stay off the closure for the whole run.

## Status of these files

Preserved unmodified, exactly as the runner left them, because a partial attempt
is diagnosed rather than overwritten. Attempt 2 runs from `6f9ab770`, which
additionally contains D2.
