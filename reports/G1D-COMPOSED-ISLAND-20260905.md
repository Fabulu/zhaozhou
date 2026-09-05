# G1-D — the composed texture island

**Date** 2026-09-05
**Top** `fpga/rtl/texture/zhao_texture_island_top.sv`
**Function** `tests/texture/island_composed_directed.cpp` — 11/11 checks
**Capacity** fit in progress; numbers land in §4 below

G1-D has two halves and they answer different questions. **Capacity** is the
fit: how much silicon the island costs once its blocks are wired to each other
instead of to pads. **Function** is whether the wiring is right at all. This
report is written function-first, because the function half found four defects
that the capacity half would have priced without complaint.

---

## 1. Why the existing island number was never the island's size

Every figure quoted for the island so far — including the 7,913 ALM in
`G1-ISLAND-SURVIVORS-20260905.md` — is a **sum of standalone per-block fits**.
Two things make that sum wrong, and they pull in opposite directions, so the
error does not even have a known sign:

* each standalone fit wraps its block in virtual pins and the registers that
  feed them, so every row carries I/O cost that disappears once the block is
  wired to a neighbour;
* nothing is shared across a block boundary — no common control, no merged
  constants, no retiming across the seam.

**The census contains its own disproof.** Totalled by row it comes to
**342 DSP blocks against the device's 112**, which is impossible. The rows
include mutually exclusive variants (`@lanes2`, `@lanes1-io`, `@pre-rearch`,
`svcseed2/3`) and no cross-block packing. A number that cannot be true is a good
place to stop adding.

`zhao_prod_top` does not answer this either, and its own header says so: it
drives every block from a **separate LFSR**. The roadmap's phrasing is exact —
"a resource-counting harness, not the console".

## 2. What the composed top is

```
  depth ---> RCP24 ---> PERSPUV ---> FRAGROB ---> COMBINE.V1 ---> out
                                       |  ^
                             tmu req   |  | sample responses
                                       v  |
                       TMU_PLAN -> CACHE_PIPE -> RSP_DISPATCH
                                                   |      |
                                             bilinear   palette
                                             BILERP     PALETTE_RES
                                       |  ^
                             aux req   |  | aux responses
                                       v  |
                                    AUX_PIPE

  MOSAIC sits on the pre-TMU u/v path.
```

Every internal signal is a real connection between two island components.
Tie-offs exist only at the island's true external boundary: the fragment stream
in from the rasteriser, the memory fill interface, the palette upload port, the
aux sheet port, and the fragment stream out.

## 3. What the composed test found

The functional test drives fragments in at the boundary and requires **every
block's counter to have moved** on the way out, each named separately so a break
is *located* rather than merely detected. That structure is what made the
debugging tractable: each failure pointed at exactly one hop.

Four defects, found in order, each hidden behind the previous one.

### 3.1 The fill model served one halfword where a line is eight beats

`zhao_texture_cache_pipe` counts beats and only marks a line valid on the last:

```systemverilog
  if (fb_beat_r == BEAT_W'(HW_PL - 1)) ... fb_busy_r <= 1'b0;
```

with `HW_PL = LINE_BYTES / 2 = 8`. The harness answered each fill with one
halfword, so the cache waited forever, back-pressured the planner, and nothing
reached the dispatcher. Reported as `cache miss 1, dispatch 0`. **A harness
defect, not RTL** — but it is recorded because a composed test's memory model is
part of the evidence, and a wrong one condemns working hardware.

### 3.2 AUX_PIPE's token was too narrow to carry the identity

FRAGROB validates every response against the slot **and generation** it issued:

```systemverilog
  assign aux_ok_c = aux_rvalid_i && val_q[aux_rslot_i] &&
                    (gen_q[aux_rslot_i] == aux_rgen_i) && auxreq_q[aux_rslot_i];
```

That is `$clog2(DEPTH) + GENW` = **12 bits**. `AUX_PIPE`'s `TOKW` defaults to
**8**, so the identity could not round-trip. `TOKW` is a parameter; it simply
had to be told how wide the identity is.

### 3.3 The sheet token was left unconnected — and it stalled the whole island

The first draft wrote `.sheet_tok_o()`. AUX_PIPE matches a sheet response to the
request that asked for it by that token, so with it dangling the responder had
nothing to echo and **every** aux result carried a wrong identity. FRAGROB
rejected all of them and counted it: 7 ID errors against exactly 7 aux requests.

The consequence is the part worth remembering. **FRAGROB retires in allocation
order**, so a single aux fragment stuck at the head blocked everything behind
it. The observable state was:

```
  plan 48 | cache hit 48 | dispatch 48 | bilerp 48 | fragrob 16 | retired 0
```

— a completely healthy sample path, 48 texels fetched and filtered, and **zero
fragments out**. Nothing in that picture points at the aux port. The per-block
counters did.

### 3.4 The recipe was not travelling with the fragment

The combiner's `recipe` / `weight` / `sample_count` were wired from the island's
**input ports**. With a reorder buffer in between, "the fragment arriving now"
and "the fragment retiring now" are different fragments *by construction*, so
every fragment was combined with somebody else's recipe. They now ride FRAGROB's
context word:

```
  [15:0] tag   [21:16] rsv   [23:22] sample_count   [26:24] recipe   [34:27] weight
```

**This one would have passed every handshake test ever written.** A wrong recipe
is a wrong picture, not a stall: counters move, fragments retire, throughput is
nominal. Only a composed test that cared about the *result* could see it, and it
is the clearest argument in this whole gate for why the roadmap demands a test
that actually draws through the added hardware.

### 3.5 And one found while writing the top: FRAGROB was returning sample 0

Not a wiring defect — a block defect, upstream of where anyone was looking.
FRAGROB banks all three sample results internally (`res_rgb_m [3][DEPTH]`) and
its retire read was:

```systemverilog
  out_rgb_r <= res_rgb_m[0][head_slot_c];
```

Banks 1 and 2 had no reader. That is precisely the *"returns sample 0 for every
recipe"* fault `MATERIAL.RESOLVE.md` attributes to the surviving TEXJOIN, living
one block earlier in the chain. FRAGROB now exposes `o_s_rgb_o[3]` /
`o_s_a_o[3]`; `o_rgb_o` / `o_a_o` are unchanged, so every existing consumer
still works.

**The near miss is worth recording.** The first draft of the top held two past
retirements in a shift register and called it a sample bank, because FRAGROB
appeared to expose only one colour. That version would have synthesised, fitted,
reported an ALM count, and blended every fragment with its two predecessors
while calling it three-sample material.

### 3.6 Result

```
  submitted 64, retired 64, fills served 8
  rcp 64 | persp 64 | plan 192 | cache hit 192 miss 1 | dispatch 192
  bilerp 192 | palette 0 | mosaic 64 | aux 41 | fragrob 64
  fragrob ID ERRORS 0
  [island_composed_directed] 11 checks passed
```

`palette 0` is expected and is not a gap in the test: the composed top tags every
request with the bilinear class, so the CLUT path is wired and idle. Exercising
it needs a class-varying workload, which belongs with the seam in §5.3.

## 4. Capacity

**Fit in progress** (`quartus_map` started 06:27:58 on the corrected top). The
number goes here with its DSP count, register count and fmax, against:

| | ALM |
|---|---|
| architecture nominal (§3.3, eleven components) | 6,600 |
| redline | 7,500 |
| sum of standalone per-block rows | 7,913 |
| **composed** | *pending* |

**No rule is registered for this target yet, deliberately.** There has never
been a composed measurement, so any threshold would be a guess dressed as a
gate — and a rule written beside the first fit it governs is the exact failure
`CLAUDE.md` records. The question gets answered before it gets gated.

Two earlier fits were killed on purpose rather than kept:

* the combiner fit, because it had snapshotted the source *before* the
  double-issue fix and therefore described a block doing twice the work;
* the first island fit, because it had snapshotted the top *before* §3.2–§3.4
  and therefore described an island that never retires a fragment.

Both were within their rights to finish and would have produced confident,
useless numbers.

## 5. Seams that remain, named rather than hidden

### 5.1 Texel-to-channel

`RSP_DISPATCH` hands the bilinear class 64 bits — four RGB565 texels.
`BILERP_LANE` consumes four **eight-bit channel values**. The top extracts one
channel; the three-channel sequencing the lane is designed for ("serial bilinear
**channel** engine") is not built, so its job counter under-reports by a factor
of three until it is.

### 5.2 Per-sample coordinates

FRAGROB takes u/v/binding/lod **per sample**. The island boundary supplies one
pair; the three samples are given the same coordinates and distinguished by
binding index (offset per sample, so the cache does not serve one line three
times and understate miss traffic). Per-sample coordinate variation is a
binding-table concern this composition does not own.

### 5.3 The response class rides the source id

`RSP_DISPATCH` needs `rsp_class_i`; `CACHE_PIPE` carries no class lane, only
`smp_src_id_o`. The class therefore travels in the **top two bits of the source
id**, which works only because `SRCW` is 16 and live source ids are far below
2^14. That is a real constraint on the id space and it was nowhere written down
before this file.

## 6. Next

1. Read the composed fit and fill §4.
2. If the composed number lands above the 7,500 redline, the survivors question
   reopens — and it reopens with a *real* number for the first time.
3. `zhao_texture_material_combine_v1` still has no measurement of its own; the
   refuted `zhao_texture_combine` stays in the tree until it does.
4. perspuv's context store (see `PERSPUV-REGISTER-DIAGNOSIS-20260905.md`) is the
   largest single register consumer in the island and is inside this fit's
   closure, so it is untouched until the fit clears.
