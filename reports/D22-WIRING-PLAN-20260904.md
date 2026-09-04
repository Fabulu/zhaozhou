# D22 — how to wire the geometry front end, one measurable step at a time

**2026-09-04, written the hour D1 closed.** D1's own rule was *"D1 still comes
first, because adding nineteen blocks to a shell that is 14 MHz short would make
attribution impossible."* That is satisfied: **100.00 MHz, zero failing
endpoints.** Both of D22's named blockers were cleared the same morning.

**The margin is 0.057 ns.** Every block added spends some of it, which is
exactly why this is staged rather than done in one commit.

## The seam is already exact, and nobody had noticed

`zhao_geom_setup`'s outputs are `zhao_geom_bin_pipe`'s triangle inputs,
**port for port**:

| `GEOM.SETUP` emits | `zhao_geom_bin_pipe` takes |
|---|---|
| `out_kx0_o` `out_ky0_o` `out_kc0_o` (x3 edges) | `tri_kx0_i` `tri_ky0_i` `tri_kc0_i` (x3) |
| `out_tl_o` | `tri_tl_i` |
| `out_ax_o` `out_ay_o` `out_bx_o` … `out_cy_o` | `tri_ax_i` `tri_ay_i` `tri_bx_i` … `tri_cy_i` |
| `out_min_x_o` `out_max_x_o` `out_min_y_o` `out_max_y_o` | `tri_min_x_i` … `tri_max_y_i` |
| `out_src_id_o` | `tri_src_id_i` |
| `out_valid_o` / `tri_ready_o` | `tri_valid_i` / `tri_ready_o` |

**Checked mechanically, not by eye** (2026-09-04) — the port lists were parsed
out of both modules and diffed by name AND width:

    SETUP  out_*_o   23
    BINPIPE tri_*_i  22
    matched           22   -- every name, every width identical
    SETUP emits but BINPIPE does not take:  ['area2']
    BINPIPE takes but SETUP does not emit:  []

**Nothing `zhao_geom_bin_pipe` consumes is unsatisfied, and no width needs a
cast.** `out_area2_o` is a passthrough of SETUP's own `tri_area2_i` that the bin
pipe does not want; it is left unconnected. This paragraph originally said "port
for port" from reading two lists side by side, which is the kind of claim that
is usually right and occasionally expensive — so it was measured.

Today `zhao_shell_top.sv:766` drives every one of those from a **shell input**
(`render_kx0_i`, `render_ax_i`, …). So the front end does not need a new
interface: it needs `GEOM.SETUP` instantiated and the multiplexer moved back one
block at a time.

## The staged order, and why it is backwards

**Compose from the BIN PIPE END, not from MESHFETCH.** The declared compose
order (`tools/design/compose_order.py`) runs MESHFETCH -> … -> SETUP, and that
is the DATAFLOW order; it is the wrong order to *build* in, because every
intermediate state would have the front end producing values nothing consumes.

Wiring backwards keeps the shell **renderable and measurable at every step** —
each stage moves the harness boundary one block earlier and the picture must not
change:

| stage | instantiate | harness now supplies | maturity of the added block |
|---|---|---|---|
| 1 | `GEOM.SETUP` | clip-space triangles | UNIT_VERIFIED |
| 2 | `+ GEOM.DEPTHQUANT` | clip-space + `w` | UNIT_VERIFIED |
| 3 | `+ GEOM.CLIP` | projected vertices | UNIT_VERIFIED |
| 4 | `+ GEOM.PROJECT`, `GEOM.WCACHE` | world vertices | UNIT_VERIFIED |
| 5 | `+ GEOM.VDECODE`, `GEOM.ASSEMBLE`, `GEOM.ASSETFETCH` | meshlet records in the pool | UNIT_VERIFIED / SPECIFIED |
| 6 | `+ GEOM.MESHFETCH` | a dispatch and a descriptor address | REFERENCE_COMPLETE |

**Stages 1-4 are all UNIT_VERIFIED blocks and need no new RTL.** That is four
measurable steps available today.

### The static path first, and lighting is NOT on it

`GEOM.LIGHT`, `GEOM.LOOM` and `GEOM.WARP` are **SPECIFIED with no RTL**, and
`GEOM.LIGHT` cannot even be started yet — the law it must not re-implement has
no signature that accepts a normal (see `design/contracts/GEOM.LIGHT.md`, and
the `shade_from_world_normal_unclamped` extraction it needs).

So stages 1-6 compose the **static, unlit mesh path**: vertices to triangles to
tiles. Skinning and lighting join later, at the `GEOM.SKIN` / `GEOM.SKIN.NORM` /
`GEOM.LIGHT` branch, which is a *parallel* limb rather than a link in this
chain. **Do not wait for lighting to start wiring.**

## Stage 1, costed exactly

`zhao_geom_setup` is a **leaf block** — it instantiates nothing — so stage 1 adds
**one** file to the shell's ordered source list (45 -> 46, and
`run_composed_fit.ps1`'s source-parity check will demand `tests/CMakeLists.txt`
agree).

**And the shell's interface gets SMALLER, not bigger.** SETUP consumes the three
screen-space vertices, the doubled area, the bounding box and `src_id`, and
computes the edge functions the shell is currently handed:

    REMOVED (9 + 1 ports)  render_kx0_i ky0_i kc0_i  (x3 edges)  render_tl_i
    ADDED   (1 port)       render_area2_i  [47:0]
    UNCHANGED              render_ax_i .. render_cy_i, min/max x/y, src_id

So the harness stops supplying **edge setup** and starts supplying **an area**.
That is the single most contained step available, and it is the one whose
"the picture must not change" claim is easiest to believe: the values SETUP
computes are the values the harness was computing by the same law.

**The area is not a new burden on the caller.** `zref` already computes it —
it is the `orient`/`area2` term the reference uses to reject degenerate and
back-facing triangles — so the harness has it in hand.

## What each stage must produce, or it does not count

1. **The picture does not change.** Every stage replaces harness-supplied values
   with computed ones that must be identical. `render_golden` and
   `reel_sequence_crc` are the evidence, and they are already passing.
2. **A composed fit.** Each stage costs ALMs and some of 0.057 ns. Stage N's fit
   is what says whether stage N+1 is affordable — the whole reason for staging.
3. **The harness boundary named in the commit.** "The shell now computes X and
   is handed Y" is the sentence that makes the next stage obvious.

## What this plan does NOT claim

* **It does not claim the stages are cheap.** Four UNIT_VERIFIED blocks entering
  a design with 0.57% margin may well push it back under 100 MHz. That is
  information, not failure — and it is precisely what D3's fit-top split exists
  to measure in isolation.
* **It does not re-derive the compose order.** `compose_order.py` owns that;
  this file only argues about the direction of assembly.
* **It does not touch `zhao_prod_top.sv`.** That is a resource top where nothing
  is wired to anything, and it stays that way.
