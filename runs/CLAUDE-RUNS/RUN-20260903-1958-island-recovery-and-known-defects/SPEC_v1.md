# RUN-20260903-1958 — island recovery and the known defects

## The owner's sequence, verbatim

> "fix animation document, whole-frame. Save the renderer first. Island
> recovery after, then count resources. As for the first animation ruling
> issue, we have to fix that. We can't fit the anims into the fast ram."

> "Then we can make a fit that actually fits something we hope is good"

> "If a step requires fit first, then of course do the fit then"

So: **fix what is known wrong → Save the Renderer → island recovery → the
production resource count last**, when it will be measuring a machine worth
measuring. A fit is run whenever a step's acceptance question needs one.

## Where the evidence lives (durable, committed, not in any context window)

| what | where |
|---|---|
| five recon digests | `reports/digests/LANE1..5-*.md` — commit `e79ea3a7` |
| the index of every outstanding instruction | `reports/DOCKET.md`, D1–D24 + the recon sweep |
| the island recovery specification | `reports/islandrearchitecture5.md` (the live one) |
| measured island vs its redline | `reports/TEXTURE-ISLAND-FIT.md` addendum + correction |
| what is left, derived not audited | `tools/ledger/remaining.py` |
| tool truth | `reports/QUARTUS_GOTCHAS.md` |

## The work, in order, with the acceptance question for each

### A. Known-wrong, NO FIT NEEDED

* **A1 `design/fit_targets.yml` resource rules.** DONE-FIRST. The island brief
  sets tripwires — cache v2 `require M10K >= 8`, `reject registers > 2,000`,
  `reject ALMs > 1,500` — but they exist only as prose, so nothing mechanical
  enforced them and **98.66 MHz was reportable as a pass while the storage law
  was violated**. Until this exists, every later fit can lie the same way.
  *Acceptance: a fit that violates a structural rule FAILS, not passes.*

* **A2 `QFMT_VERSION` skew.** 3 in `zref_tables.hpp`, `tools/fixgen/src/fixp.ts`,
  `compiler/src/generated/tables.ts`; 2 in `runtime/include/zhao_abi.h`,
  `fpga/rtl/generated/zhao_abi_pkg.sv`, `compiler/src/generated/abi.ts`. R3
  ordered the 2→3 bump; the `abi` generator never got it. Capture-visible
  numeric law disagreeing across the hardware/software boundary.
  *Acceptance: one value everywhere, from the generator, not hand-edited.*

* **A3 `MATERIAL_RECIPE_VERSION = 1`** (R9) exists only in prose — no header,
  package or table.

* **A4 pose cache `sub`** — **DONE**, commit `a11aed18`, 59 checks.

### B. Save the Renderer  ·  `SaveTheRendered.md` (repo root)

Owner: *"After the islands, this is next"* — but sequenced FIRST in the later
ruling. It is "Save the **Renderer**", 99.50 → 105 MHz. **Not yet read in
full.** First action: read it, extract its steps, put them here.

### C. Island recovery  ·  `reports/islandrearchitecture5.md`

The root cause is ONE construct and the fix is written inside the block being
replaced.

* **C1 `zhao_texture_cache_pipe` storage.** Reads at `:302-306` are correctly
  synchronous; the **writes** at `:412`/`:416` sit inside the async-reset
  process at `:309`. **An M10K has no reset port.** `blockMemoryBits: 128`
  against the predecessor's 8,192. The predecessor records the A/B in a comment
  at `zhao_texture_cache.sv:495-523`: static lane index alone changed nothing
  (5,402 → 5,373 ALM, zero M10K both), the clock-only process inferred 4 M10K
  at 1,087 ALM / 1,737 reg.
  *A coding-style port, NOT a rewrite: C0–C4, replay, multicast, counters stay.*
  **Acceptance (brief §25): if M10K does not infer, STOP. Needs a fit.**

* **C2 `zhao_raster_texjoin_v2`.** Same disease, plus a combinational read at
  `:332-341` and two dynamic write addresses into one array (`:390`, `:468`),
  which the brief's §5.3 forbids by name. 7,056 bits vs 7,151 measured regs.

C1 + C2 alone are ~79% of the ALM and ~75% of the register recovery.

* **C3** FRAGROB banking, **C4** perspuv, **C5** rcp24 — per the brief's ordered
  ledger, landing on §3.3's nominal 6,600 ALM / 6,050 reg.

### D. The production resource count — LAST

`zhao_prod_top` + `design/prod_manifest.yml` are built and the top elaborates
with all 64 blocks. Re-run when C is done, so it measures a machine worth
measuring rather than one with a known 11,000-ALM defect inside it.

## Standing constraints that bind this run

* **The fit reads the LIVE tree** (gotcha 11). No editing a fit source while a
  fit runs. Killed fits skip their cleanup `finally` — sweep the workspaces.
* **`REJECT: adding terrain/Field RTL faster than the texture fit can be
  closed`** — island brief. So no new terrain RTL until C closes.
* **The normal-map gate is the art law**: the amended oracle goes in the zref
  renderer and the owner looks at the island under a moving sun BEFORE any RTL.
  The current draft is quarantined and known-wrong.
* Read `reports/QUARTUS_GOTCHAS.md` before touching RTL that has never been
  through the fitter. Three of its entries were rediscovered by probe today.
