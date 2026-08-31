# TASK_LOG — RUN-20260831-0213-renderer-attribute-path

The renderer's attribute path: build it, compose it, and price it against a
frame. Continued from a prior context; this folder was created part-way through,
so the entries before it are reconstructed from the commits they produced.

## What landed

| commit | what |
|---|---|
| `f5350af` | RASTER.ATTRDIV — the divide, exact rounding, 36 clocks measured |
| `9764d2e` | RASTER.ATTRDIV.SVC — tagged service, UNITS sweep, in-order retire |
| `25d96d2` | RASTER.INTERP — the plane stepped onto pixel CENTRES |
| `401bac5` | GEOM.CLIP — attributes follow their vertices through the winding flip |
| `cf61d17` | RASTER.RCP24 — full 16,777,215-input domain against the frozen hash |
| `615ed5f` | RASTER.PERSPUV — the perspective divide, shift derived then attacked |
| `8418364` | RADIX becomes a parameter: 36 clocks -> 20 |
| `6abc31e` | TEXJOIN — internal sequence identity, AUX concurrent at zero cost |
| `e7d8818` | THE SEAM — five blocks together, 1,644 pixels, exact |
| `270f436` | GEOM.WCACHE shell (the owner ruling's outstanding half) |
| `43fe72e`, `b3bfdca` | reports/PER_PIXEL_BUDGET.md |
| `db02ada` | reports/OPEN-SPEC-DEPTH-QUANTISATION.md |
| `8a353a6` | tools/render/count_fragment_load.cpp — overdraw measured |
| `54168d3`, `e093171` | reports/CONSOLE_REMAINING.md, and its correction |
| `a0bc76d` | CORRECTION: edgewalk setup is 5 clocks, not 21 |

## Evidence

* 125/125 on the raster, geometry, render, ledger, lint and formal gates.
* `ledger:check` green: 92 blocks, V1-V17 + V19-V23 + staleness.
* Every block's own directed test green; each new check mutation-tested where a
  mutation was meaningful.

## Measurements that changed a decision

* the divide: 36 clocks -> 20 at radix 4; the UNITS x RADIX grid reaches
  658,978 divides a frame;
* `rcp_u24` matches `RCP24_FULL_HASH` over its entire input domain;
* AUX runs concurrently with the primary TMU at **zero** added clocks
  (1,446 either way);
* overdraw measured exactly: 1.00x for a full-screen pass, 2.20x for an army;
* edgewalk setup is 5 clocks, which reprices ruling 4 from 32% of a frame to
  7.67%.

## Three things I got wrong, and how each was caught

1. **Edgewalk setup measured over the wrong interval.** Accept-to-first-beat is
   21 clocks and contains the whole 16-row walk. My self-check -- that the figure
   was identical across three coverages -- could not have failed, because the
   walk is always 16 rows. Caught by re-reading the state machine. Corrected in
   `a0bc76d`, with the wrong reasoning kept in the test header.
2. **Overwrote an existing 268-line test.** `geom_wcache_directed.cpp` already
   existed; the Write tool said "updated" and I did not read it. Caught by CMake
   refusing a duplicate target, not by me. Restored from git.
3. **Claimed seven blocks were buildable.** They are not: four contracts are
   stubs with 15 TODO sections, one is a documented refusal, and two need data
   formats that do not exist. Caught by opening the contracts, which the first
   pass never did. Corrected in `e093171`.

4. **A regression I introduced and did not see for a day.** `da6ca7a` landed
   ruling 2 (one lifecycle per tile) and updated two render tests but not
   `terrain_project_chain`, which composes the same `bin_pipe`. It went red and
   stayed red because every gate I ran was scoped to `raster_|render_|geom_` and
   that file is `terrain_` -- and the three full-label runs I started were each
   killed before finishing. Diagnosed by reverting the three files of `da6ca7a`
   until it passed, which cleared both my GEOM.CLIP widening and the creature
   lane's merge. The RTL was right; the test encoded the old law. Fixed in
   `4c76318`.

   **The lesson is about the scope, not the bug.** A scoped gate drawn around
   the blocks being EDITED misses the blocks that CONSUME them. 177/177 now,
   over a sweep wide enough to have caught it.

## Open, and all of them decisions rather than work

1. `wmin`, `wmax`, `scale` — blocks GEOM.PROJECT's attribute carry.
2. The binner arena capacity.
3. What `276,480` counts — decides whether two per-pixel blocks need replicating.
4. Seven contracts to write, or a ruling on who may write them.

---

## Wave A, after bro's second architecture pass (owner rulings 2026-08-31)

Rulings recorded durably at `reports/OWNER-RULINGS-20260831.md`.

### Landed

| commit | what |
|---|---|
| `f8c2b32` | the rulings, and the fit project found STALE |
| `3de0bc7` | PROOF: exact attribute stepping, 10x fewer divides -- and the ruling's stated formula is NOT the shipped law |
| `fa5cbc5` | DEPTH PROFILES derived and proved; scale comes out 2^40 / 2^39 / 2^38 |
| `7ca89bf` | fit: source list generated from CMake, 141 virtual pins |
| `128dd36` | fit: the unpacked-array ports, element by element |
| `a11c53c` | ATTRDIV publishes its Euclidean remainder |
| `01e8ac4` | RASTER.ATTRSTEP: the recurrence in RTL, 15.1x fewer divides, not one bit moved |
| `feca129` | the divide crisis superseded |

### The fit, honestly

Analysis & Elaboration and Analysis & Synthesis both pass, 0 errors -- **the
shell plus the whole render path elaborates and synthesises, which had never
been shown.** The FITTER does not yet complete:

* first attempt: source parity refused it -- the QSF listed neither
  `zhao_geom_bin_pipe` nor `zhao_raster_fbwrite` nor their closure, though
  `zhao_shell_top` instantiates both. 28 -> 42 sources, generated from CMake so
  parity holds by construction.
* second: `742 IO input pads against 315 available` -- the 40 wide render ports
  were not virtualised. Named explicitly, 101 -> 141.
* third and fourth: `156 user-specified I/O pins against 145 available`, and
  the count did NOT move when the ten unpacked-array ports were virtualised
  element by element. **So the remaining 156 are something else and I have not
  yet identified them.** A local fit is running to produce the I/O assignment
  report rather than guess a fifth time.

**742 -> 156 against a 145 limit is real progress and not a result.** The fit
number this run owes is a completed fitter and a TimeQuest Fmax, and it does not
have one yet.

### Errors this wave, all mine

1. **Ran the fit against uncommitted QSF edits** and read the unchanged 742 as
   "the virtual pins do nothing". `run_shell_fit.ps1` snapshots from
   `git archive HEAD` deliberately, so a fit names the exact commit it measured.
2. **Generated 2,446 bogus virtual pins** from a regex that matched packed
   vectors and captured the word `logic` as a port name.
3. **Tracked the signed quotient in ATTRSTEP** and stepped the negative branch
   backwards; the directed test caught it on the second pixel.
4. **Left a refused ATTRSTEP job unable to emit**, so it hung instead of
   flagging.
5. **Built an exact-half test case with zero exact halves in it**; only the
   anti-vacuity check noticed.
6. **Asked ATTRSTEP for a quotient of 156 billion** against a stated 32-bit
   precondition -- the block refused correctly and the test was wrong.

### The fit wall came down on the fifth attempt

`156 user-specified I/O pins` was **eight ports declared two to a line**:

```
input logic signed [22:0] render_kx0_i, render_ky0_i,
3 x 23 (ky0/1/2) + 3 x 21 (ay/by/cy) + 2 x 12 (max_x/max_y) = 156
```

A first-identifier-per-line scan virtualised every x half and left every y half a
pad. **The names were in `output_files/zhao_shell_fit.fit.rpt` section 12 the
whole time** -- I did arithmetic on the number three times instead of generating
the report once.

`823e703` makes that class of failure impossible: `run_shell_fit.ps1` now asserts
virtual-pin parity BEFORE any Quartus stage and prints the offenders by name.
Mutation-tested by deleting `render_ky0_i` alone, which is exactly the port that
cost three attempts. It also refuses if it parses fewer than 50 ports, so a
broken regex fails loudly rather than passing vacuously.

```
PASS source parity: 42 ordered shell sources match tests/CMakeLists.txt.
PASS virtual-pin parity: 186 shell ports all virtualised.
```

`5393924` writes the whole staleness class down in
`fpga/quartus/FIT-PROJECT-STALENESS.md`, beside the project rather than in this
run folder, because it recurs whenever a block gains a top-level port.

### Still owed

The fit NUMBER. Elaboration and synthesis pass with 0 errors and the full render
path; the fitter has run past the I/O wall but has not yet produced ALM, memory
and Fmax. The flow is running locally so its reports persist on disk regardless
of what happens to a wrapper process -- five background runs were killed by the
harness during this wave.
