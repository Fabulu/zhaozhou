# Texture island: does the 99.5 MHz renderer survive the texture path?

**Live document. Rows land as fits complete.** Started 2026-09-02.

## Why this and not terrain first

Owner direction, 2026-09-02:

> "the important bit is actually fitting all the texture stuff to see if the
> 99.5 MHz renderer and full fitted console actually holds up or if it needs
> more reingeneering. Keep your eyes on the prize."

and, after the reviewer's defect list landed:

> "bro is finding many issues with the outstanding stuff, so focus on the fit
> for now, we really need to know if the renderer holds up"

Ten texture-island blocks were written and functionally verified and **none of
them had ever been fitted**. Functional verification says a block computes the
right numbers. It says nothing about whether it can be clocked.

## What a row here is and is not

Per the brief's Phase 1: **a leaf fit is reconnaissance. It does not authorize
production integration.** And per `run_block_fit.ps1`'s own limitations, which
travel with every number below:

* `5CSEBA6U23I7` is a **provisional** target, not board truth.
* **All I/O is virtual.** No package pins, no board I/O delays, no PLLs.
* A per-block fit says nothing about the composed machine's routing.
* Nothing here is a programmed device.

And the one that matters most for reading these numbers honestly:
**placement noise is 4.61 MHz**, measured across three seeds on identical RTL
(`reports/NOISE_FLOOR.md`). A single-seed row is a draw from a distribution.
Differences smaller than ~5 MHz between two rows are not differences.

A **`timeout` row is not a timing result.** It means we did not wait. The first
`tmu_plan` attempt returned `timeout 3385.8s` against a 3000 s budget with a
formal proof competing for CPU; re-run alone at 9000 s it fitted in 2524 s.

## Targets, from the brief's Phase 4

| stage | target |
|---|---|
| leaf datapaths | designed against **150 MHz** |
| perspective/TMU/cache/AUX island | **120–125 MHz**, three seeds |
| texture-survivor composition | **115–120 MHz** |
| full composition floor | **105 MHz** |

The shipped shell is **99.50 MHz best / 96.87 mean**. A leaf that fits below
that number is a leaf that sets the console's clock.

## Rows

| block | status | Fmax | ALM | reg | M10K | DSP | vpins | secs |
|---|---|---|---|---|---|---|---|---|
| `zhao_texture_tmu_plan` | ok | **93.55** | 1419 | 1054 | 0 | 0 | 363 | 2524 |

### `zhao_texture_tmu_plan` — 93.55 MHz

**Below the shell's own 99.50 MHz, and 56 MHz below the 150 MHz leaf target.**

This is the five-stage elastic TMU plan pipe (T0–T4). It is the first hard
number on the texture island and it is not a good one: the block that plans a
texture fetch cannot be clocked as fast as the renderer that would use it.

Read it with the caveats above — one seed, so ±4.61 MHz, and 363 virtual pins
is a lot of unconstrained fabric I/O for a leaf. But the gap is 12× the noise
floor. This is not a placement draw.

**Zero M10K and zero DSP** on a block that plans texture addressing is worth
noting on its own: everything here is fabric.

*(remaining rows land as the queue completes: cache_pipe, perspuv_svc,
texjoin_v2, rcp24_svc, aux_pipe, palette_res, rsp_dispatch, bilerp_lane,
mosaic)*
