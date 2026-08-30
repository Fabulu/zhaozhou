# The GEOMETRY -> RASTER seam, first time the two halves ran together

Written 2026-08-30 from `tests/render/render_pipe_directed.cpp`, which drives
`zhao_geom_binner` straight into `zhao_raster_tile_pipe` through the composition
that already existed for it, `fpga/rtl/geometry/zhao_geom_bin_pipe.sv`. Nothing
in this file is modelled: every number is measured on the composed RTL against
`zref::Binner` and `pipe_oracle` in series.

**A correction that belongs at the top, because it changes what two of the
findings below are findings ABOUT.** This work began by writing a NEW probe,
`zhao_probe_render_pipe.sv`, to join the binner to the tile pipe -- without
first checking that `zhao_geom_bin_pipe.sv` had done exactly that since phase 5,
with its own directed test. The duplicate has been deleted and this file now
drives the real composition, which needed only five observability outputs added
to it (`jobs_taken_o`, `job_stall_clocks_o`, `arena_full_o`,
`early_z_rejects_o`, `fragment_error_o` -- counters, no behaviour).

So of the two "composition bugs" recorded below, the **tile index units** one
was a bug in the duplicate probe I wrote, not in the machine: the shipped
`zhao_geom_bin_pipe` has always formed the index correctly, and by a different
law again (packed `{row, col}`, not row-major). It is kept here because the
trap is real and the next composition will meet it -- but it is not a defect
found in the console. The **ledger timing** one is a genuine harness trap that
any consumer of `tile_crc_index_o` can fall into. The **one-resolve-per-job**
finding is a property of the machine and stands unchanged.

Search the tree before building a composition. This one cost an afternoon and
would have cost a shipped duplicate.

    [render_pipe_directed] 15 checks passed

| section | what it drives | measured |
|---|---|---|
| 1. one triangle over many tiles | 31 jobs, sink always ready | 31 tiles, 12,825 clocks, 7,847 job stalls |
| 2. two overlapping triangles | 40 jobs over 29 distinct tiles | 40 tiles, 40 jobs taken, 10,116 job stalls |
| 3. stuttering framebuffer sink | `fb_ready_i` refused on an LFSR schedule | no tile lost, 8,206 job stalls |

---

> ## FIXED 2026-08-30. The decision was option 1.
>
> The owner chose "accumulate several triangles into one tile before resolving",
> and it is built. GEOM.BINNER now emits `job_first_o` and `job_last_o` from its
> drain cursor -- which already knew where a reference sat in a tile's list --
> and RASTER.TILE_PIPE clears the front bank on the FIRST reference of a tile
> and swaps-and-resolves on the LAST.
>
>     section 2, before   40 resolves over 29 tiles, the second clear erasing
>                         the first triangle
>     section 2, after    29 resolves from 40 jobs, both triangles composed
>     framebuffer         10,240 px in 640 bursts -> 7,424 px in 464 bursts
>
> A caller that ties both bits high gets exactly the old behaviour, which is why
> the tile pipe's own 74 directed checks are unchanged and still mean what they
> did. `pipe_oracle` grew the same two flags so the oracle models the machine.
>
> The section below is kept as the record of how it was found and what the three
> options were.

## The finding that matters: a shared tile is rendered TWICE, and the second render erases the first

`zhao_raster_tile_pipe` states its own scope at the top of the file: **"no
multi-triangle accumulation into one tile (one job = one clear + one triangle +
one resolve)"**. That is a per-block statement and it is honoured exactly. What
only appears once a BINNER is upstream is the consequence at frame scale:

* two triangles that both reference tile (3,2) produce **two jobs**, not one;
* each job clears the tile store, walks its own triangle, and streams **all 256
  pixels** to the framebuffer;
* so the second stream writes the CLEAR word over every pixel the second
  triangle does not cover -- including the pixels the first triangle had just
  written.

Section 2 measures 40 resolve beats over 29 distinct tiles and the test's oracle
now models the machine as it is: one `PipeTile` per job, tiles row-major, and
within one tile the triangles in submission order. The first run of this gate
collapsed the jobs per tile and reported 40 against 29 -- **that was the oracle
being wrong about the design, not the design being wrong.**

**This is not a bug report and no behaviour has been invented to fix it.** With
depth test off and blend REPLACE (state 0, what these sections drive) the
overwrite is total. What the frame is SUPPOSED to look like when two triangles
share a tile is a design decision:

1. accumulate several triangles into one tile before resolving (the tile store
   already ping-pongs, so this is a scheduling change in the binner's drain, not
   a new datapath), or
2. resolve per job but have the framebuffer write path honour a per-pixel
   PRESENT mask so uncovered pixels are not written at all, or
3. keep it and let the frame be built by the depth test with the clear word
   carrying a far depth -- which needs the depth-enabled state, not state 0.

Option 1 is the one the tile store's ping-pong was built for. **The choice is
the owner's and this file does not make it.**

---

## Two composition bugs, both invisible from either bench

**The tile coordinate is a pixel origin, not a column.** `job_tile_x_o` is the
tile's top-left PIXEL and both blocks read it that way, so the seam itself is
correct. But the row-major CRC index the composition has to FORM from it is not
in the same units, and multiplying the pixel origin by the grid width stamped
every tile with an index sixteen times too large. The binner never forms an
index and the pipe is handed one, so neither bench could see it. Fixed in
`zhao_probe_render_pipe.sv` with the shift stated at the assignment.

**The tile ledger is not ready at the last pixel.** `tile_crc_o`,
`tile_crc_index_o` and `tile_cov_count_o` become the finishing tile's on
`tile_done_o`, which trails `fb_last_o`. Sampling them at the last framebuffer
beat pairs this tile's pixels with the previous tile's ledger. The coverage
count still matched -- it is re-derived per tile -- so the symptom was ONLY the
index, one behind on 30 of 31 tiles. A harness that had checked the count and
not the index would have shipped green.

Both are harness/composition faults. **No RTL inside either block was changed.**

---

## A third one, in the test rather than the machine

The whole chain speaks screen coordinates with **eight** fractional bits
(`zhao_raster::px(p) = p * 256`), and this file was first written with `<< 4`.
Every triangle then landed inside a single tile, so section 1 rendered one tile,
section 2 rendered two, and both agreed with the oracle. **Fifteen checks were
green over a picture that was 1/16 the intended size** -- the oracle and the RTL
were wrong in exactly the same way, which is what a shared-units bug does. The
tell was the section title: "one triangle spanning several tiles", measuring 1.

---

## Back-pressure, which was the other reason to build this

Section 3 refuses `fb_ready_i` on an LFSR schedule, so the resolve stage
back-pressures the tile store, which back-pressures the fragment path, which
back-pressures the pipe's `job_ready_o`, which stalls the binner's drain. That
chain runs across the seam and **cannot be built with a bench on either side** --
each block alone faces a partner that is always ready, or one that refuses on a
schedule the bench itself chose.

Measured: 8,206 job-stall clocks, every tile delivered, every picture exact.

Section 1 is the same measurement without the stutter: 7,847 job stalls in
12,825 clocks. **The binner spends 61% of the frame waiting for the pipe**, with
a perfectly cooperative sink. That is the throughput fact this seam exists to
expose, and it is the number to attack when the render path gets its own
optimisation pass -- the geometry side is not the cost.
