# DECISION — `ProjectedVertex` SCHEMA v2, and alpha is **s22**

PVSCHEMA, 2026-09-27. Branch `gz/pvschema`, base `e9c384d3`.
Standing authority: `reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt` §0 and §4
(*"The architect has explicit authority to amend record schemas for this
purpose."*).
Decision-record format per §0: question; chosen option; reason and alternatives;
constraints/cost; code/tests/compatibility consequences.

---

## QUESTION

SWAPBUILD established that entry `I55`'s five-times-quoted premise is false: the
24-byte `ProjectedVertex` does **not** carry what the six Packet-D planes need,
because `zhao_geom_vertid.sv:499` stores colour through `unit8_of_fx16` — a
lossy `(v+128)>>8` with rails at both ends — while `zhao_geom_attrpack` reads the
**full 32-bit attribute slots**. The three Gouraud planes owner ruling R234 D1
added cannot be rebuilt from the v1 record by **any** back end.

So the blocker is a **record**, not an architecture. This record answers:

1. What is the v2 layout, and where is it declared?
2. **How wide is alpha?** The obvious v2 (`x21 y21 invw24 status8 u/w32 v/w32
   r32 g32 b32 alpha32`) is **266 bits** and does **not** fit the 256-bit slot.

---

## THE BUDGET IS 256 BITS, AND IT IS HARD

`PV_STRIDE_B` is **32** while the record is **24** — every vertex slot already
carries eight bytes of declared slack, so a wider record costs **no address
space**: `VERT_CAP_B` does not move and no region in `spec/memory_rules.md` §5c
changes.

**32 bytes is a ceiling, not a preference.** A 48-byte stride would take the
view's used footprint from 3,407,872 to 4,456,448 bytes against a
`ZHAO_PARAMBUF_VIEW_SPAN` of 4,194,304. It does not fit, and §0 forbids reaching
a fit by shrinking a declared maximum. **256 bits is the whole budget.**

---

## THE MEASUREMENT THAT DECIDES ALPHA

Alpha's width was decided by measuring **what reads the field** and **what
writes it**, not by taking what was left over.

### Nothing downstream interpolates alpha, and the RTL says so by name

`zhao_geom_attrpack.sv:141-146`, the waiver on its own unread slot:

> *"Packet-D's carriage is now SIX planes — invw24, u/w, v/w and the three
> Gouraud channels — so the only slot this block does not ask for is `alpha`
> (slot 6), whose value is governed by ruling R48's named `ALPHA_C` constant and
> whose PER-PRIMITIVE producer is `tri_continuation_tail_i`'s `vertex_alpha`
> (ruling R89)."*

So **alpha is not a plane input.** The six planes are `invw`, `u/w`, `v/w`, `r`,
`g`, `b`. The alpha that reaches the blend is an 8-bit unit8 off the continuation
tail on a *different* path — `zhao_console_core.sv:10762`: *"`ALPHA_C` IS NOT ON
THE BLEND'S PATH AT ALL."*

### Every live producer of every colour slot is Q0.16 with 1.0 = `0x1_0000`

Measured at all four attribute-packet producers, not assumed:

| producer | slots 3/4/5 (r/g/b) | slot 6 (alpha) | max value | bits |
|---|---|---|---|---|
| `zhao_geom_vattr` (mesh) `:534-537` | `{15'd0, lit[16:0]}` | `ALPHA_C = 32'h0001_0000` | `0x1_0000` | **17** |
| `zhao_terrain_clipfeed` `:760-783` | `lit_of_mod` → `{16'd0, s[15:0]}` | `TERR_ALPHA = 65536` | `0x1_0000` | **17** |
| `zhao_part_clipfeed` `:535-541` | `lit_of_byte` → `{16'd0, c, 8'd0}` | `PART_ALPHA = 65536` | `0x1_0000` | **17** |
| `zhao_forge_assemble` `:681-684` | `art_r/g/b_i`, 32-bit ports | `art_alpha_i`, 32-bit port | see below | **17** |

`zhao_forge_assemble`'s four colour ports are `signed [31:0]` and are driven from
named composer constants — `zhao_console_core.sv:11557-11559, 11632-11634,
11706-11708, 11702`:

    FORGE_LIT_R  = 32'sd62259   (0.95)      SHADOW_LIT_R = 32'sh0000_1800
    FORGE_LIT_G  = 32'sd63897   (0.975)     SHADOW_LIT_G = 32'sh0000_1800
    FORGE_LIT_B  = 32'sd65536   (1.0)       SHADOW_LIT_B = 32'sh0000_2000
    CLIFF_LIT_R  = 32'sd36044   (0.55)      SHADOW_ART_ALPHA = 32'sh0000_6000
    CLIFF_LIT_G  = 32'sd34406   (0.525)
    CLIFF_LIT_B  = 32'sd32768   (0.50)

**The largest value anywhere in the tree is `0x1_0000`, which needs 17 bits.**
`zhao_geom_vertid.sv:396` states the same law in its own words: *"the lit
channels occupy its low 17 bits and alpha its low 17 too."*

---

## CHOSEN OPTION

**`ProjectedVertex` v2 — 32 bytes, 256 bits, declared once in `zhao_pkg`.**

| bit offset | width | field | signed | why this width |
|---|---|---|---|---|
| 0 | **21** | `screen_x` | s | the domain `pv_illegal_o`/`fits_s21` **already refuses** outside of. Writing down a law the hardware enforces, not a truncation. |
| 21 | **21** | `screen_y` | s | ditto |
| 42 | **24** | `invw24` | u | R7, unchanged. Plane input, exact. |
| 66 | **8** | `status` | u | R7's ratified byte. `GEOM.VERTID.md` §169 defines `[3:0]`; `[7:4]` stay **reserved, written 0, nonzero is malformed** — unchanged, and they are the record's reserve. |
| 74 | **32** | `u_over_w` | s | **plane input — stored EXACTLY, no domain claim** |
| 106 | **32** | `v_over_w` | s | plane input — exact |
| 138 | **32** | `gouraud_r` | s | plane input (R234 D1) — exact |
| 170 | **32** | `gouraud_g` | s | plane input — exact |
| 202 | **32** | `gouraud_b` | s | plane input — exact |
| 234 | **22** | `alpha` | s | **not a plane input** (attrpack never reads slot 6). Its entire declared domain is 17 bits; s22 carries that plus a sign bit and **5 bits of overbright headroom**, and is the whole remainder of the slot. |
| | **256** | | | **32 bytes exactly** |

### ALPHA IS **s22**, AND THE REASON IS A MEASUREMENT, NOT A LEFTOVER

The reason is recorded in the record itself (`zhao_pkg.sv`, beside the constant)
and is this:

> **Alpha is the ONLY field narrowed below its slot, because it is the only one
> with no plane consumer.** `zhao_geom_attrpack` reads slots 0..5 and explicitly
> not slot 6. Every one of the six quantities the planes are built from is
> therefore stored at its **full 32-bit slot width** and needs **no domain claim
> that could be wrong**. Alpha's full declared domain across all four producers
> is `0x1_0000` — 17 bits — so s22 is the domain plus a sign bit plus 5 bits of
> headroom, and it is what the slot has left after fidelity is bought where it
> is load-bearing.

### WHY NOT THE OTHER WAY ROUND

The tempting alternative — narrow `r`/`g`/`b` to their measured 17-bit domain and
give alpha 32 — was **rejected**, and the reason is this codebase's own law about
which direction an error runs.

Narrowing a **plane input** to a measured domain makes the record's one job — *the
six planes come out bit-identical* — conditional on a claim about every present
and future producer. `art_r_i` is a 32-bit port driven by a **named, editable
owner constant**; CLAUDE.md rule 6 is explicit that *"every shape, colour and
timing value belongs in a named, editable constant"*, and a record that silently
caps that constant at 17 bits **removes the owner's control in the name of
fidelity**. Narrowing **alpha** — which no plane reads — risks nothing the planes
depend on.

**No claim at all beats a claim with a large margin.** The six plane inputs carry
zero domain claims in v2.

### THE SCHEMA IS VERSIONED AT ELABORATION, NOT PER RECORD

§4 asks for *"a versioned extension … keyed by the same identity."*
`ZHAO_PARAMBUF_PV_SCHEMA = 2` is a single constant that the encoder and the
decoder both derive from, with an elaboration guard that they agree.

A per-record version **nibble was considered and rejected**: the only place it
could go is `status[7:4]`, which `GEOM.VERTID.md` §169 rules *"reserved, written
0. Nonzero is a malformed record"* — so spending it would repeal a live legality
rule to encode something no reader needs. v1 and v2 records never coexist in one
arena: one build writes it and the same build reads it back within the frame.

---

## A LATENT DEFECT THE OBVIOUS v2 WOULD HAVE TRIPPED

`zhao_geom_paramarena.sv:1195` sets the record's SDRAM burst length as

```systemverilog
  m_beats_q <= 4'(PV_B / 8);
```

**an integer division with no guard that `PV_B` is a multiple of 8.** The block's
elaboration section (`:655-685`) refuses six separate alignment breaches by name
and does **not** refuse this one.

The layout this packet's own brief proposed is *"242 bits = 30.25 bytes"*. At
`PV_B = 30` or `31` that expression yields **3 beats = 24 bytes**: the record is
allocated 32 bytes, declared 30, and **24 are written**. The last field is
dropped into a slot the reader will then decode as whatever SDRAM held, with **no
diagnostic anywhere** — `m_len_q` would say 30 while `m_beats_q` says 24.

It is the flattering direction again: a short write completes, the counters all
balance, and the missing bytes read back as plausible numbers.

**v2 is 32 bytes, so it does not trip it — and the guard is added anyway**, with a
committed positive control, because a guard that holds by luck is one that should
say so out loud (the block's own words at `:678`).

---

## THE DETECTOR THAT GOES BLIND, DECLARED RATHER THAN LEFT READING ZERO

`zhao_geom_parambuf`'s `pv_illegal_o` today asserts when a decoded `screen_x` or
`screen_y` fails `fits_s21`. In v2 **x and y are stored as s21**, so a decoded
coordinate is legal *by construction* and that term can never fire again.

This is exactly CLAUDE.md's law — *"a detector reading zero is a claim, and it is
the claim to check hardest"* — so it is handled explicitly rather than left as a
reassuring zero:

* the s21 **refusal moves to the encoder**, which is the only place the 32-bit
  value still exists to be judged. `zhao_geom_paramarena` gains `pv_narrow_o`,
  counted per vertex, and the record is **refused, not clamped** — clamping would
  place a triangle somewhere plausible;
* `pv_illegal_o` **keeps its port and its meaning** and now watches the field that
  can still be malformed on the way back — it is not repurposed and not deleted;
* neither counter is quoted without having been **seen to fire**.

**The illegal state has been made unrepresentable, which is a strengthening.** It
is written down because "the counter reads zero" and "the counter cannot move"
are different facts and only the second is true here.

---

## CONSTRAINTS AND COST

**No address space.** `VERT_CAP_B` unchanged, no `spec/memory_rules.md` §5c
region changes, `PV_STRIDE_B` unchanged at 32. `PV_STRIDE_B >= PV_B` still holds,
now with equality.

**SDRAM write traffic on the vertex arm rises 33%**, and this is the one real
cost. The record goes from 3 beats to 4 — 24 to 32 bytes actually written into a
slot that was already 32 bytes allocated. At the smoke fixture's 213 vertices that
is 1,704 extra bytes per frame; at R7's 65,536-vertex giant it is 512 KiB per
frame. **Declared, not absorbed.** It is unavoidable: the bytes are the function.

**No new SDRAM share slot.** This packet adds no memory client. Both shares are
full — `zhao_geom_mem_adapter` is `N=10` with all ten driven (its own header says
"NINE" and is stale by one) and `u_geom_wshare` is `N=3` with all three driven —
and v2 rides the write path the arena already owns.

**No DSP, no new arithmetic.** v2 is a layout, not a computation. The quantiser
`unit8_of_fx16` is **retained**, because `zref::unit8_from_fx16` is a published
law with a directed test differencing every channel against an independent
transcription; what changes is that it stops being the **only** thing the record
keeps.

---

## CONSEQUENCES — CODE, TESTS, COMPATIBILITY

**Code.** The layout is declared **once**, in `fpga/rtl/common/zhao_pkg.sv`, as
field offsets and widths rather than only a size — today `zhao_pkg` carries the
three record **sizes** and nothing about their fields, so the encoder
(`zhao_geom_paramarena.sv:804-812`) and the decoder
(`zhao_geom_parambuf.sv:199-206`) are two hand-maintained inverses whose own
comment says *"the two must agree bit for bit."* After this they derive from the
same constants and cannot disagree.

`zhao_geom_vertid` stops being the record's precision floor: it gains
full-precision colour outputs beside the retained `pv_rgba_o`. That is a port
change on a leaf, so `zhao_prod_top.sv`, `zhao_console_board.sv` and the shell
paired diff are regenerated and **every bench that instantiates the changed
modules is connected** — *"a port on a leaf costs its WHOLE instantiation chain
plus every bench."*

**Tests.** The evidence bar is a **round trip at full precision that FAILS
against v1**: a colour whose low eight fractional bits are non-zero and which is
therefore *not* recoverable through `(v+128)>>8`. `{255,255,255}` proves nothing
and is not used.

**Compatibility.** No ABI change — `spec/commands.zidl` is untouched, so
`npm run abi:check` is not implicated. R7's *record versus allocation stride*
distinction is preserved exactly: the stride does not move.

**The three committed arena mutants are COPIES of `zhao_geom_paramarena` and are
refreshed by three-way merge, not transplant** — a copy of a record that no
longer exists is a positive control for a block that does not exist. Each carries
its own mutation forward and nothing else, and each is shown to still FIRE.

---

## WHAT THIS RECORD DOES NOT CLAIM

It does not claim `I55` is closed, that the time multiplex is built, or that any
pixel yet depends on bytes that went through SDRAM. It claims that the **schema
blocker SWAPBUILD measured is removed**: after v2 the six Packet-D planes are
reconstructible from the arena record at full precision, which they were not
before, by any back end.
