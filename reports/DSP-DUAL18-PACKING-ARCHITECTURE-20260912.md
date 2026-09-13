# Cyclone V dual-18x18 packing architecture

Date: 2026-09-12
Architecture only; no RTL, build, simulation, map, fit, commit, or production-adoption work was performed by this packet.

The committed source audit is against repository HEAD `a569a73571475cce0caf5fcd0ce639a4245a93d3`. The resource starting point is the **111-DSP conditional structural frontier** in `reports/CEILING-FRONTIER-RECONCILIATION-20260912.md`. It is not the illegal 99-DSP RPP=1 point and it is not a composed receipt.

## Executive decision

**Explicit dual-18 packing is a real architecture candidate, but not yet a demonstrated implementation.** Three independent kinds of evidence line up:

1. Altera's Cyclone V documentation says a variable-precision DSP block supports **two independent 18x18 multiplications**. The Cyclone V SE A6 row is 112 physical variable-precision DSP blocks, 336 independent 9x9 multipliers, and **224 independent 18x18 multipliers**. The 336 value belongs to the 9x9 column, not the 18x18 column.
2. Quartus 17.0.2's installed `cyclonev_mac` metadata lists the legal mode string `m18x18_full`, two operand pairs, two result-width parameters, two result ports, and independent signedness parameters for all four operands.
3. The existing projector fit charges `Total DSP Blocks = 33 / 112`, and its DSP-mode rows also sum to 33. Its 22 rows named `Two Independent 18x18` have the B-side data-register columns marked `--`, while its logical fixed-point multiplier count is 44. This is strong evidence that each of those inferred rows occupies a physical block while leaving its second independent multiply lane unused.

That settles the accounting semantics and makes the recovered lead credible. It does **not** settle whether the exact Quartus-17 parameter combination maps, whether both result ports survive, whether mixed signedness is bit-exact in the encrypted atom, or whether moving wide-product recombination into ALMs is affordable. One tiny MapOnly discriminator must settle the physical premise before any production RTL changes.

The preferred boundary is a **combinational, stateless, direct `cyclonev_mac` wrapper** with a separately selected portable behavioral backend. `altera_mult_add` is rejected for this boundary because the installed RTL exposes one combined `result`, not two independent products.

If the primitive gate passes, the legal RPP=3/MATW=18 projector can plausibly move from structural **24 DSP to 11 DSP** without changing II or arithmetic. If it fails, none of that 13-DSP saving may be banked.

## 1. What Quartus is counting

### 1.1 Device documentation

The official references supplied with the commission are:

- [Cyclone V resources table](https://docs.altera.com/r/docs/683375/current/cyclone-v-device-handbook-volume-1-device-interfaces-and-integration/resources?contentId=m1eTGdYr~hIQ930EPymwPg)
- [Supported operational modes in Cyclone V devices](https://docs.altera.com/r/docs/683375/current/cyclone-v-device-handbook-volume-1-device-interfaces-and-integration/supported-operational-modes-in-cyclone-v-devices?contentId=P_g8Sh12N70nsgY7jCUdgg)
- [Cyclone V Device Overview: Variable-Precision DSP Block](https://docs.altera.com/r/docs/683694/current/cyclone-v-device-overview/variable-precision-dsp-block?contentId=DfL21bJ5g7LHAndq9mObpQ)

The relevant distinction is:

| quantity | Cyclone V SE A6 |
|---|---:|
| physical variable-precision DSP blocks | 112 |
| independent 9x9 multiplication instances | 336 |
| independent 18x18 multiplication instances | 224 |
| independent 27x27 multiplication instances | 112 |

Thus `112` and `224` are not competing limits. They are the physical-block count and the logical 18x18-lane count respectively. Each physical block has two 18x18 lanes in the independent mode.

### 1.2 Quartus fit evidence uses the same physical denominator

`reports/synthesis/blockpaths/zhao_geom_project.fit.rpt` identifies Quartus Prime Lite 17.0.2 Build 602, Cyclone V `5CSEBA6U23I7`, and reports:

```text
Total DSP Blocks  33 / 112
```

The same report's `Fitter DSP Block Usage Summary` says:

```text
Two Independent 18x18             22
Sum of two 18x18                  11
Total number of DSP blocks        33

Fixed Point Signed Multiplier      9
Fixed Point Unsigned Multiplier   13
Fixed Point Mixed Sign Multiplier 22
```

The mode counts are counts of physical DSP rows: 22 + 11 = 33, matching the fitter total and the device's physical denominator. The fixed-point counts are logical multiplication operators: 9 + 13 + 22 = 44. Those are deliberately different accounting layers.

The detail rows at `zhao_geom_project.fit.rpt:4767-4799` each occupy a distinct `DSP_X*_Y*_N0` location. In every `Two Independent 18x18` row shown there, the `Data BX Input Register` and `Data BY Input Register` columns are `--`; `Sum of two 18x18` rows instead show live A- and B-side columns as `no` or `yes`. This is not merely a suggestive mode label: it is physical-row evidence that the B half is absent in those inferred independent-mode rows.

The eleven old full-width products account for 44 logical 18x18 partial operators. Eleven `Sum of two 18x18` physical blocks absorb 22 of those partials. The other 22 partials consume 22 separate independent-mode physical blocks, one live lane per block. That is the three-block shape for each old wide product.

The independent calibration corroborates the narrower case: `tools/budget/calibration.json` records `calib_mulasym_s32x18_ioreg` as two logical multipliers, two `Two Independent 18x18` blocks, and `dspBlocks = 2` for one inferred signed32-by-signed18 expression. Inference has not recovered the available pairing.

### 1.3 Exact conclusion, and its limit

- **Settled:** Quartus `Total DSP Blocks` and the 112 device limit refer to the same physical variable-precision DSP resource.
- **Settled for the cited fit:** the 22 inferred `Two Independent 18x18` rows use only one multiply half each.
- **Not generalized without evidence:** a mode row named `Two Independent 18x18` does not by its name alone prove that one or both lanes are live. The detail ports and logical-operator reconciliation are required.
- **Not yet settled:** one hand-instantiated Quartus-17 atom with both outputs live maps to exactly one such physical row. That is the discriminator's job.

## 2. Smallest reusable exact boundary

### 2.1 Choice among primitive and IP options

| construction | two independent outputs | local Quartus-17 evidence | decision |
|---|---|---|---|
| inferred SystemVerilog `*` expressions | logically yes | repeatedly maps unrelated products separately | baseline only; not an architecture |
| `altera_mult_add` | no: one main `result` | installed `quartus/libraries/megafunctions/altera_mult_add_rtl.v` | reject for reusable two-result boundary |
| direct `cyclonev_mac` | declaration has `resulta` and `resultb` | installed atom declaration and XML metadata | preferred, conditional on the gate |
| another generated native fixed-point DSP IP | none found locally that exposes this exact two-result contract | local installation search only | not selected; absence is not claimed beyond this installation |

The installed declaration at `C:\intelFPGA_lite\17.0\quartus\eda\sim_lib\cyclonev_atoms.v:4323-4535` exposes `ax`, `ay`, `bx`, `by`, `resulta`, and `resultb`, plus `signed_max`, `signed_may`, `signed_mbx`, and `signed_mby`. Its behavior delegates to `cyclonev_mac_encrypted`, so this declaration is interface evidence, not functional proof.

The stronger local parameter evidence is `C:\intelFPGA_lite\17.0\quartus\libraries\megafunctions\xml_info\cyclonev_mac_info.xml`: line 52 lists `m18x18_full` as a legal `OPERATION_MODE`; lines 56-62 expose both result widths and all four signedness controls; lines 71-87 expose both operand pairs and both results.

The first packet must use **the installed Quartus-17 metadata**, not copy a primitive recipe from a newer online release. The authoritative combination is obtained by all of:

1. retaining the installed XML/declaration excerpts in the calibration receipt;
2. generating the simulator-library setup with Quartus 17's own library tooling for the selected supported simulator rather than guessing encrypted-file order;
3. observing a successful Quartus-17 map of the candidate parameter set;
4. observing one physical independent-mode row in the map report and both results live in the mapped hierarchy; and
5. running the same vectors through the actual encrypted vendor model.

A successful map is authoritative for parameter legality and resource shape. Vendor-model differential simulation is authoritative for primitive behavior. Neither substitutes for the other.

### 2.2 Boundary contract

The proposed reusable module is `zhao_dual18_mul` with four raw 18-bit operand ports and two raw 36-bit result ports:

```text
ax_i[17:0] * ay_i[17:0] -> resulta_o[35:0]
bx_i[17:0] * by_i[17:0] -> resultb_o[35:0]
```

Each of the four operands has an **elaboration-time** signedness parameter. Signedness is never dynamic per transaction. A narrower signed operand is sign-extended to 18 bits before the boundary; a narrower unsigned operand is zero-extended. The result is interpreted as signed if either operand on that lane is signed, otherwise unsigned. The 36 output bits always carry the exact full product.

The boundary has deliberately **no** clock, reset, enable, valid, ready, tag, rounding, saturation, truncation, accumulator, or internal pipeline state:

| property | contract |
|---|---|
| state | none |
| combinational latency | zero registered cycles; outputs settle from current inputs |
| initiation interval | one independent pair of products per caller clock, subject to timing |
| clock/reset/CE | absent |
| stalls | caller holds or ignores arithmetic exactly as it does now |
| result width | exact 36 raw bits per lane |
| operand signedness | four static parameters, one per primitive operand |
| pairing legality | both products must share the caller's stage, clock, reset/enable decision, and latency contract |

This is smaller and safer than imposing one registered wrapper on callers with different contracts. The projector already owns rigid pipeline registers; FIELD has registered input and output; cull and normals register products; TESS and LOD sequence a combinational product in their own control. A common internal register would change at least one of those schedules and would entangle the primitive's clock-enable/reset modes with production behavior.

No products may be paired across clock domains, separately stallable interfaces, unrelated production roots, or different pipeline stages. The absence of CE from this boundary is what makes that rule explicit rather than pretending two independent controls fit one physical block.

### 2.3 Candidate `cyclonev_mac` settings

The exact candidate settings to map are:

```text
operation_mode       = "m18x18_full"
ax_width             = 18
ay_scan_in_width     = 18
bx_width             = 18
by_width             = 18
result_a_width       = 36
result_b_width       = 36
operand_source_max   = "input"
operand_source_may   = "input"
operand_source_mbx   = "input"
operand_source_mby   = "input"
signed_max           = lane-A X signedness
signed_may           = lane-A Y signedness
signed_mbx           = lane-B X signedness
signed_mby           = lane-B Y signedness
ax_clock             = "none"
ay_scan_in_clock     = "none"
bx_clock             = "none"
by_clock             = "none"
output_clock         = "none"
use_chainadder       = "false"
enable_double_accum  = "false"
ay_use_scan_in       = "false"
by_use_scan_in       = "false"
preadder_subtract_a  = "false"
preadder_subtract_b  = "false"
```

All other data/control inputs are tied inactive: `az`, `bz`, `scanin`, `chainin`, coefficient selects, `loadconst`, `accumulate`, `negate`, and `sub` to zero; `clk` and `aclr` to zero because every clock parameter is `none`; `ena` to all ones. Unused `scanout`, `chainout`, and `dftout` are left unconsumed. Width-one unused-port parameters are made explicit so a default change cannot alter elaboration.

These are **candidate** values until `dual18_physical_pack_discriminator` maps and the vendor model agrees. In particular, the existence of `m18x18_full` in XML does not by itself prove that this complete setting produces two independent outputs on this exact tool/device.

### 2.4 Portable behavioral backend and exclusive elaboration

Use two explicit backend-selection macros:

```text
ZHAO_DUAL18_CYCLONEV
ZHAO_DUAL18_BEHAVIORAL
```

Exactly one must be defined. The source contains one preprocessor-exclusive implementation:

```systemverilog
`ifdef ZHAO_DUAL18_CYCLONEV
  `ifdef ZHAO_DUAL18_BEHAVIORAL
    zhao_dual18_backend_conflict MUST_NOT_ELABORATE();
  `endif
  // exactly one cyclonev_mac
`elsif ZHAO_DUAL18_BEHAVIORAL
  // exact portable products
`else
  zhao_dual18_backend_not_selected MUST_NOT_ELABORATE();
`endif
```

The deliberately unresolved module names make absent/conflicting selection an elaboration failure in both Quartus and Verilator. There is no runtime mux and never a netlist containing both implementations.

The behavioral branch should not depend on fragile expression-context signedness. For each operand, construct a signed 19-bit mathematical value: sign-extend an 18-bit signed input or prepend zero to an unsigned input. Multiply the two signed 19-bit values into 38 bits and emit the exact low 36 bits. For legal 18-bit operands, the upper two bits are necessarily the sign extension of bit 35 for a signed/mixed product or zero for an unsigned product; simulation assertions check that invariant. This gives one portable expression for unsigned, signed, and mixed cases while preserving the primitive's raw 36-bit contract.

### 2.5 Source-list rules

| use | macro | sources/libraries | forbidden |
|---|---|---|---|
| Quartus synthesis/map | `ZHAO_DUAL18_CYCLONEV=1` in QSF | wrapper and selected calibration/production callers; `cyclonev_mac` is a built-in atom | adding `cyclonev_atoms.v` as ordinary synthesis RTL; behavioral macro |
| Verilator/reference | `ZHAO_DUAL18_BEHAVIORAL=1` | wrapper, calibration shell, ordinary testbench | vendor macro; parsing/elaborating encrypted atom |
| vendor functional simulation | `ZHAO_DUAL18_CYCLONEV=1` | wrapper and shell plus Quartus-17 generated simulator-library setup, including the appropriate `cyclonev_atoms` encrypted library | behavioral macro; a hand-guessed mixed simulator library list |
| inferred MapOnly contrast | neither wrapper nor vendor atom | inferred baseline top only | including an explicit wrapper in the baseline total |

For Quartus the QSF assignment is the tool's Verilog-macro assignment for `ZHAO_DUAL18_CYCLONEV=1`. CI must inspect the effective source list and macro set. A production QSF that accidentally selects the behavioral backend would silently return to inference and is a gate failure even if arithmetic tests pass.

No ModelSim/Questa executable was found under the installed `C:\intelFPGA_lite\17.0` tree. An Intel-supported simulator/library setup is therefore a named prerequisite, not something this architecture assumes exists.

## 3. Exact arithmetic families

### 3.1 Notation and typing law

- `uN(x)` means interpret the N bits of `x` as unsigned.
- `sN(x)` means interpret them as two's-complement signed.
- `zxW(x)` and `sxW(x)` mean explicit zero/sign extension to W bits **before** shifting or adding.
- Every left shift below is performed only after extension to the stated accumulator width. This avoids SystemVerilog's self-determined shift width discarding high bits.
- All products are full precision. Only an already-existing caller may perform its original later rounding, rescale, saturation, or truncation.

### 3.2 Two unrelated products of at most 18 by 18

For each operand independently, extend to 18 bits according to its declared signedness. One `m18x18_full` atom computes:

```text
PA36 = AX18 * AY18
PB36 = BX18 * BY18
```

The lanes may use different static signedness combinations. Unsigned-by-unsigned is interpreted as unsigned36. If either operand is signed, the product is interpreted as signed36. Mixed 18-bit signed/unsigned extrema fit signed36 exactly.

This is the only family that can save a DSP with **no new wide recombination**. It is the first production preference.

### 3.3 Signed32 by signed18 in one physical block

Let `A` be signed32 and `B` signed18. Split `A` at bit 16:

```text
L = u16(A[15:0])
H = s16(A[31:16])
A = L + 2^16 H
```

Compute the two simultaneous lane products:

```text
pL35 = s17({1'b0,L}) * s18(B)   // exact signed35 declaration
pH34 = s16(H)         * s18(B)  // exact signed34 declaration
```

The physical operands are sign/zero-extended to 18 as required, and each raw lane output is 36 bits. Recombine in signed50:

```text
P50 = sx50(pL35) + (sx50(pH34) << 16)
```

Then `P50` is exactly `s32(A) * s18(B)`. The formula includes `A = INT32_MIN`, `B = INT18_MIN`, and the positive product `2^48`; no absolute-value transform is used, so the signed minimum has no exceptional case.

One wide product consumes two lanes of one physical DSP block. The fabric receives one signed50 addition. Whether Quartus's inferred two-DSP form already pays an equivalent fabric carry chain must be recorded in the discriminator map rather than assumed.

### 3.4 Projector viewport: signed32 by unsigned12, then exact shift

Current committed `zhao_project_core.sv:922-923` computes each axis as:

```text
ndc * (viewport_extent << 15)
```

The extent is the current unsigned 12-bit `vp_w` or `vp_h`. Integer multiplication distributes over a left shift exactly, and the current 64-bit MAD domain has enough range, so:

```text
ndc * (V << 15) = (ndc * V) << 15
```

For signed32 `N` and unsigned12 `V`, split `N`:

```text
L = u16(N[15:0])
H = s16(N[31:16])
N = L + 2^16 H

pL28 = u16(L) * u12(V)   // unsigned28
pH28 = s16(H) * u12(V)   // signed28
Q44  = zx44(pL28) + (sx44(pH28) << 16)
R64  = sx64(Q44) << 15
```

`Q44` is exactly signed32-by-unsigned12. `R64` is exactly the old mathematical product; its magnitude is below the signed64 limit. There is no intermediate truncation or rescale. Each axis consumes one dual-lane block, so the two axes consume two blocks total.

The existing stage-5b product register was added because this viewport cone was already critical. The replacement must land `R64` in the same `s6_prod_x/s6_prod_y` registers under the same rigid `en_i`; adding or moving a cycle is not permitted by this architecture.

### 3.5 Signed32 by signed32

#### Schoolbook, two physical blocks

```text
A0 = u16(A[15:0])       A1 = s16(A[31:16])
B0 = u16(B[15:0])       B1 = s16(B[31:16])
A  = A0 + 2^16 A1
B  = B0 + 2^16 B1

p00 = A0 * B0            // unsigned32
p01 = A0 * B1            // signed32
p10 = A1 * B0            // signed32
p11 = A1 * B1            // signed32

T66 = zx66(p00)
    + (sx66(p01) << 16)
    + (sx66(p10) << 16)
    + (sx66(p11) << 32)
P64 = T66[63:0]
```

The exactness check is `T66[65:64] == {2{T66[63]}}`. Four 16x16 partials occupy two dual-lane blocks. Recombination should first combine the two cross terms, then use an implementation chosen for the caller's register boundary; the equation does not authorize a new pipeline stage.

#### Karatsuba-like, still two blocks for one product

```text
sA18 = zx18(A0) + sx18(A1)
sB18 = zx18(B0) + sx18(B1)
z0   = A0 * B0
z2   = A1 * B1
zm   = sA18 * sB18
cross = zm - zx(z0) - sx(z2)
P = z0 + (cross << 16) + (z2 << 32)
```

`A0 + A1` ranges from -32768 to 98302 and fits signed18. Three lane products still round up to two physical blocks for one standalone product, leaving one lane unused. For `N` same-stage products with common control, the bank cost can fall from schoolbook `2N` to `ceil(3N/2)` blocks by pairing Karatsuba partials across products.

Karatsuba adds two 18-bit pre-adds per wide product, two wide subtractions to recover `cross`, and a deeper recombination cone. No saving is available for a standalone product, so it is rejected there. It is only a later bank option if a subsystem fit shows ALM and timing margin.

### 3.6 Signed33 by signed33

Split at bit 16:

```text
A0 = u16(A[15:0])       A1 = s17(A[32:16])
B0 = u16(B[15:0])       B1 = s17(B[32:16])
A  = A0 + 2^16 A1
B  = B0 + 2^16 B1

p00 = A0 * B0            // unsigned32
p01 = A0 * B1            // signed33
p10 = A1 * B0            // signed33
p11 = A1 * B1            // signed34

T68 = zx68(p00)
    + (sx68(p01) << 16)
    + (sx68(p10) << 16)
    + (sx68(p11) << 32)
P66 = T68[65:0]
```

The upper two temporary bits must sign-extend `P66[65]`. All four operands fit a dual-18 lane, so schoolbook costs two physical blocks per wide product.

For Karatsuba, `A0 + A1` and `B0 + B1` range from -65536 to 131070 and fit signed18 exactly. Therefore three partial products suffice and a bank costs `ceil(3N/2)`. As for 32x32, one standalone product remains two blocks and Karatsuba only adds fabric there.

A signed33-by-signed32 site may use the same schoolbook structure with the second high limb signed16, producing the exact signed65 result. Sign-extending the signed32 operand to 33 and checking the discarded top product bit is sign extension is also legal; implicit 64-bit carrier widths are not.

### 3.7 Current 34-bit terrain forms

#### Signed34 by signed34 schoolbook

Use radix `2^17`:

```text
A0 = u17(A[16:0])       A1 = s17(A[33:17])
B0 = u17(B[16:0])       B1 = s17(B[33:17])
P68 = A0*B0
    + ((A0*B1 + A1*B0) << 17)
    + ((A1*B1) << 34)
```

All four partial products are at most 17x17 and consume two blocks. Recombine in a signed70 temporary and require sign extension above bit 67.

A straightforward Karatsuba middle operand is `A0 + A1`, whose maximum is 196606; that needs signed19. It does not fit an 18-bit lane. Therefore schoolbook is the preferred exact 34x34 form.

#### TESS signed34 by unsigned17 in one block

Committed `zhao_terrain_tess.sv:792-794` has signed34 `m_d`, unsigned17 `j_morph`, and a signed52 product. Split only the signed operand:

```text
L = u17(m_d[16:0])
H = s17(m_d[33:17])
pL34 = L * j_morph       // unsigned34
pH34 = H * j_morph       // signed34
P51  = zx51(pL34) + (sx51(pH34) << 17)
m_prod52 = sx52(P51)
```

Both partials fit one dual-18 block. The current rescale consumes the same signed52 value. No rounding point moves.

The selected six-DSP TESS charge is old evidence. A future 6-to-1 observation would include stale-width/inference correction as well as dual-lane packing; it must not be described as five DSP recovered solely by pairing two lanes.

### 3.8 Texture PERSPUV signed32 by unsigned24

For the conservative schoolbook form, split signed32 at 16 and unsigned24 at 12:

```text
A0 = u16(A[15:0])       A1 = s16(A[31:16])
B0 = u12(B[11:0])       B1 = u12(B[23:12])
P56 = A0*B0
    + (A0*B1 << 12)
    + (A1*B0 << 16)
    + (A1*B1 << 28)
```

Four lane products cost two blocks per axis. The simultaneous U/V pair therefore moves from fitted six to a conservative four blocks.

An exact Karatsuba-like alternative splits `B` at 16 (`B0=u16`, `B1=u8`). `A0+A1` fits signed18 and `B0+B1` fits unsigned17, so each axis needs three lane products. Across the simultaneous U/V pair, six lane products can occupy three blocks. This is a possible 6-to-3 variant, but it adds pre-add/subtract networks and is not in the preferred lattice until the 6-to-4 schoolbook form has a subsystem ALM/timing receipt.

## 4. Current legal candidate inventory

Costs below are physical DSP blocks. `Structural` means counted from current sites and measured width calibrations, not a receipt for the current composition. `Mapped` and `fitted` retain their provenance caveats. Every packed target is unimplemented and unmeasured at the time of this report.

| current candidate | present live shape and cost | preferred packed target | ALM/register/timing direction | behavior and schedule law | evidence class; measurement boundary |
|---|---|---:|---|---|---|
| `zhao_project_core`, legal RPP=3/MATW=18 | nine signed32x18 sites at 2 each plus two old viewport sites at 3 each = **24** | **11** | nine signed50 recombinations; two signed44 recombinations plus wiring shift. No new registers. Matrix risk medium; viewport risk high because this cone already required a cut | preserve rigid `en_i`, II=1, stage-5b/stage-6 latency, exact MAD bits, refusal law | 24 is structural on implemented/unfitted shared composition; measure at projection/terrain composition gate, not leaf |
| `zhao_field_v3_mulbank`, four `zhao_field_mul` lanes | four simultaneous signed33x33; about **12**, but **unpriced in the 192/111 bill** | schoolbook **8**; optional bank Karatsuba **6** | multiple 66-bit fabric compressors; no added cycle allowed; very high ALM/timing concern | each lane accepts every cycle and responds on the existing two-cycle cadence; preserve II=1 | source-structural only; full FIELD subsystem fit; any result is an omitted positive charge, not a subtraction from 111 |
| current `zhao_geom_skin`, default `MUL_LANES=3` | three simultaneous signed32x32; **9** | schoolbook **6**; optional Karatsuba **5** | three wide recombiners; Karatsuba adds pre-add/subtract depth. Existing ancestor is only 89.65 MHz, so risk is high | preserve three-lane workload, II=12, and product schedule; `MUL_LANES=1` is a deliberately failing throughput point and is not a lever | current source shape plus fitted ancestor; one batched geometry subsystem fit |
| current `zhao_geom_cull`, default two lanes | two simultaneous signed33x33; structural **6** | schoolbook **4**; optional Karatsuba **3** | two wide recombiners; medium/high timing risk | preserve two-lane workload, II=22, and existing product latency | current behavior/rate verified, DSP structural; same geometry subsystem fit |
| `zhao_terrain_bake_v2` | one muxed signed34x34 site; conservative frontier charge **6** | schoolbook **2** | one wide 68-bit recombiner; ALM and Fmax uncertain; no new registers assumed | preserve one-site sequencing and product-register enable; no new latency | implemented/functionally verified but unfitted; existing terrain gate T1 |
| `zhao_terrain_normals` | one sequenced signed33x33 site; current clean map **3**; old 18-DSP fit is superseded | schoolbook **2** | wide recombination is dangerous: this path already gained a product register and an older TESS+NORMALS path was about 31 MHz | no temporal-sharing claim; preserve the existing sequencer and product register | current mapped DSP, fitted area/timing unresolved; terrain gate T1 |
| `zhao_terrain_lod` | one sequenced unsigned32x32; fitted **3** | schoolbook **2** | wide unsigned recombiner; medium/high risk | no temporal sharing remains; preserve schedule and result width | fitted width shape; terrain gate T1 |
| `zhao_terrain_tess` geomorph | one signed34xunsigned17 expression; selected old charge **6** | **1** | one signed51 add; likely favorable DSP delta, but old cost makes ALM/delta attribution uncertain | preserve signed52 `m_prod`, existing rescale, and job schedule | expression is current; selected fit is stale/mixed-age; terrain gate T1 |
| current `zhao_geom_lod` | one sequenced multiply carried through 64-bit expression types; selected **6**, while actual per-state values are at most signed33x32 | **2**, only after independent range proof | large apparent saving, but narrowing and fabric recombination both create high correctness/ALM/timing risk | prove every state's operand bounds and preserve exact original result/schedule; no temporal-sharing claim | fitted historical charge plus current-source range observation; geometry subsystem fit |
| current pose `zhao_geom_mat3x4_mul` | default one sequenced signed32x32 site; structural **3** | **2** | one wide recombiner; medium risk | preserve sequenced default and current pose-decode rate; do not revive obsolete three-lane form | current structural; geometry subsystem fit |
| `zhao_raster_rcp24_mul` in RCP v3 | four parallel 16x16 partial products already recombined in fabric; current map **3** | **2** | **no new arithmetic compressor**; replace pairing only; lowest risk | preserve all existing registers, correction, valid/tag, and rate | current map and live texture composition; texture-island-v3 fit |
| `zhao_raster_perspuv_pairpipe` | simultaneous signed32xunsigned24 U/V; fitted **6**, 794 ALM, 119.25 MHz | schoolbook **4**; optional pair Karatsuba **3** | schoolbook adds/reshapes wide recombination but has timing margin; Karatsuba higher ALM/depth | preserve paired acceptance, valid/tag/stalls, and latency | fitted child and current island instantiation; same texture-island-v3 fit |
| `zhao_texture_bilerp_lane` | same-stage `pu0` and `pu1` signed9x9 plus later `pv_c`; **3** | **2** | pair `pu0/pu1` with no new compressor; low risk | both products already share stage and ready/valid gating; `pv_c` remains separate | current source and live island composition; texture-island-v3 fit |
| `zhao_texture_material_combine_v2` | simultaneous two 8x8 products; structural/dirty-fit **2** | **1** | no new wide compressor; rounding additions remain fabric as now; low risk | preserve common register enable and exact `+128`, rounding, saturation | current source and live island composition; texture-island-v3 fit |

### 4.1 Why projector 24 to 11 is plausible

The arithmetic is direct:

```text
9 row sites * (2 -> 1) = 18 -> 9
2 viewport sites * (3 -> 1) =  6 -> 2
                                  -----
                            24 -> 11
```

This does not use RPP=1 and does not reduce the retained dense two-view workload. RPP remains 3 and each vertex still advances every enabled clock.

Any of these observations prevents the claimed 11:

- `m18x18_full` is rejected by Quartus 17 for the chosen ports/widths;
- one explicit pair maps to two physical blocks, or one lane maps into ALMs;
- `resultb` is optimized away, tied to `resulta`, or not independently functional;
- any signed, mixed-sign, signed-minimum, or viewport vector differs from the independent oracle;
- the exact signed32x18 assembler maps to more than one DSP;
- a helper multiplication is accidentally inferred in recombination;
- the explicit boundary cannot preserve the current `en_i` hold and stage latency;
- added fabric carry chains push the composed projector over its clock target or make ALM movement unacceptable;
- the current 12-bit viewport extent premise changes;
- the vendor functional simulation branch cannot be exercised before production migration; or
- the shared projector remains unadopted or fails its existing composition gate.

The MapOnly discriminator can settle the first five physical/functional premises. Only the existing projection/terrain subsystem fit can settle timing, ALM, and adoption.

### 4.2 Texture subtotal

Current committed island composition keeps these children live. After the already-banked RCP-v3 change, the current structural subtotal is approximately:

```text
RCP24 v3 multiply       3 -> 2
PERSPUV pairpipe        6 -> 4   (conservative schoolbook)
bilerp lane             3 -> 2
material combine v2     2 -> 1
                       -------
texture island         14 -> 9
```

The selected clean 17-DSP island fit predates the current RCP reduction; 14 and 9 are structural, not current composed receipts. The optional PERSPUV Karatsuba pair could make the target 8, but it is deliberately not banked in the preferred lattice.

### 4.3 Struck and conditional non-candidates

- **`zhao_geom_quat2mat`: struck as a current saving.** The current default is already `MUL_LANES=1`, one sequenced 16x16 site and one DSP. Its old nine-product/nine-DSP fit remains useful proof that inference did not co-pack unrelated narrow products; it is not money in the current frontier.
- **Obsolete spatial mat3x4/pose variants: struck.** Only the current one-site `zhao_geom_mat3x4_mul` wide decomposition is retained; no workload or II is reduced.
- **`zhao_geom_setup` and `zhao_geom_clip`: struck.** Their relevant operands exceed an 18-bit lane and already use one physical 27x27-mode block per product. Dual-18 decomposition would not return a block.
- **`zhao_geom_fogfactor`: unbanked.** A signed34x32 three-DSP site exists, but no meaningful current functional composition was found outside production-census scaffolding.
- **`zhao_raster_fog`: unbanked.** It has three narrow channel products, but `prod_manifest.yml` says no composed top instantiates it yet.
- **Shell-attributed sites: struck from this report.** D3 shell characterization is separate and in progress. No value is inferred from the stale shell fit, and `zhao_shell_top.sv` is not part of this architecture's edit or fit plan.
- **`zhao_forge_cliff`: conditional only.** Its two simultaneous narrow address products could be a 2-to-1 pair, but no meaningful functional parent was found. It cannot enter the frontier merely because the generated census top names it.
- **Already-sequenced multipliers:** no further temporal-sharing saving is claimed. Terrain normals, terrain LOD, pose mat3x4, and geometry LOD remain candidates only for exact internal wide-product decomposition.
- **Cross-root pairing: forbidden.** Two products in unrelated production roots do not become pairable because their counts are adjacent in a spreadsheet.

## 5. Both ceilings: selection order and resource consequences

### 5.1 Ranking by DSP returned per added fabric, then timing risk

No numeric DSP-per-added-ALM ratio is honest before the discriminator maps and subsystem fits. The useful pre-fit ranking is qualitative:

1. **Idle-lane exposure with no new compressor:** RCP's existing four partials (3-to-2), bilerp's same-stage pair (3-to-2), material-combine-v2's pair (2-to-1), and only if composition appears, forge's address pair. These should add almost no arithmetic ALM; they merely make physical grouping explicit.
2. **Signed32x18 exact recombination:** the nine projector row sites. Inference already decomposes each site into partials, so this is likely favorable, but the exact old fabric/DSP-adder split must be compared rather than assumed.
3. **Projector signed32xunsigned12 viewport factorization:** large DSP return, only one 44-bit combination per axis, but high timing sensitivity because this was already a critical cone.
4. **PERSPUV schoolbook 6-to-4:** current 119.25-MHz evidence offers some timing margin, but it introduces explicit wide recombination. Do schoolbook before Karatsuba.
5. **Standalone wide schoolbook forms:** TESS, geometry LOD, bake v2, normals, terrain LOD, and pose mat3x4. Their DSP return is attractive, but each may replace an internal DSP adder arrangement with long ALM carry chains.
6. **Multi-lane schoolbook banks:** cull, skin, FIELD. They instantiate several wide compressors simultaneously and can dominate ALM/routing.
7. **Karatsuba banks:** only after a schoolbook subsystem receipt shows that another DSP is worth the additional pre-add/subtract logic. Skin and its already-low 89.65-MHz ancestor make Karatsuba particularly suspect.

A saved DSP accompanied by enough ALM or Fmax damage to miss 30,000 ALM is not a saving. MapOnly proves physical existence, not usability.

### 5.2 What moves into fabric

`m18x18_full` provides two raw products, not a free wide-result compressor. Explicit schoolbook creates:

- one result-width shifted addition for signed32x18;
- one result-width cross-term addition plus final shifted additions for 32x32/33x33/34x34;
- explicit sign/zero extension and carry-chain routing; and
- potentially more fanout from limb inputs.

Quartus's inferred wide modes may currently use dedicated internal adders or chains. The fitted projector's `Sum of two 18x18` rows demonstrate that such internal accumulation is real. Therefore a theoretical one-DSP reduction can increase both ALM and path delay. Every production packet must compare matched before/after **ALM, registers, DSP, M10K, worst path, and Fmax** at its named subsystem boundary.

No extra register stage is authorized merely to recover timing. If a subsystem only meets timing by changing externally visible latency, that variant fails this architecture and needs a separate behavioral architecture review.

### 5.3 M10K is not an ALM exchange rate

The combinational primitive boundary adds no state. Its small caller product registers and calibration valid/tag registers are too shallow to justify an M10K and often cannot infer one without changing read latency or enable semantics.

M10K can help only where a genuine indexed/deep storage structure already exists and preserves behavior—for example terrain-bake-v2's meets plane, whose one-M10K inference is already a separate T1 question. That expected inference is already part of the 111 frontier dependency and cannot be counted again as payment for DSP packing.

Do not create queues merely to put new registers in M10K. A latency queue changes behavior, costs control ALM, and does not erase the wide recombination carry chains that are the actual risk. Global unused-M10K figures, especially from mixed-age evidence, authorize no generic ALM trade.

## 6. Decisive first gate: `dual18_physical_pack_discriminator`

### 6.1 Pre-registered question

On Quartus Prime Lite 17.0.2 Build 602 targeting Cyclone V `5CSEBA6U23I7`:

1. Do two independent, full-width 18x18 results from one explicit `cyclonev_mac` consume exactly **one total physical DSP block**?
2. Does the exact signed32x18 assembled result, using both lanes, also consume exactly **one total physical DSP block**, with recombination in ALMs and no helper DSP?

This is MapOnly. No fitter is needed to answer it, and no timing conclusion may be drawn from it.

### 6.2 Three separately compiled calibration revisions

All revisions use an identical registered shell: independent operand ports, output ports, `valid`, and a changing tag. The shell has synchronous active-high reset with reset priority and one common `ce_i`. On every enabled edge it accepts new operands/valid/tag and advances the prior registered operands' result/valid/tag to the outputs; on `ce_i=0`, **all** input, output, valid, and tag state holds while external inputs continue to change. The arithmetic boundary itself remains combinational. Latency is one enabled transfer from accepted input registers to output registers; stalls extend wall-clock latency without changing transaction order.

Compile separately so totals are not contaminated:

1. **`dual18_inferred_pair`** — two ordinary independently inferred registered products. Expected contrast: two DSP blocks. If Quartus unexpectedly maps this to one while preserving both results, record that good result and reconsider whether a vendor atom is necessary; do not call it a failure.
2. **`dual18_explicit_pair`** — one `zhao_dual18_mul` vendor instance, both 36-bit results separately live. Required: one DSP block.
3. **`dual18_s32x18_exact`** — one vendor instance supplies the low/high partials and a signed50 fabric recombination. Required: one DSP block.

Each top has independent nonconstant inputs and separately observable product outputs. Do not hash both products into one commutative signature that could hide a lane swap. Preservation attributes may keep the calibration hierarchy, but constants and duplicate operands are forbidden.

### 6.3 Functional gates before MapOnly

One deterministic corpus feeds both the portable and vendor simulations:

- exhaustive per-lane 8x8 input pairs for unsigned/unsigned, signed/signed, and both mixed-sign directions, embedded with correct extension into 18 bits; lanes use different deterministic permutations so they change independently (there is no infeasible four-operand Cartesian requirement);
- full 18-bit boundary Cartesian sets including signed minimum, signed maximum, adjacent values, `-1`, `0`, `1`, unsigned maximum, and independent lane changes;
- deterministic full-domain randomized dual-lane vectors with all four operands changing independently;
- signed32x18 boundary Cartesian vectors, explicitly including `INT32_MIN`, `INT18_MIN`, both maxima, zero, and mixed extrema, plus deterministic random vectors;
- signed32xunsigned12 projector vectors at all signed32 boundaries and viewport `0`, `1`, `2`, `2047`, `4094`, `4095`, plus deterministic random vectors;
- back-to-back valid traffic, isolated bubbles, and CE stalls of lengths 1, 2, and long randomized bursts while source operands and tags continue changing;
- reset asserted while valid and while stalled, using the shell's declared reset priority.

Expected values come from an independent widened integer oracle, not the behavioral wrapper expression under test. Compare every output and tag, not only a digest. Also emit a deterministic transcript/digest so the generic Verilator run and the Intel-supported vendor-model run can be compared without making Verilator parse an encrypted atom.

Required positive controls:

1. A functional control swaps `resulta/resultb` or changes exactly one signedness setting. Independent lane vectors must make the oracle comparison fail.
2. A CE control advances one valid/tag/product register while `ce_i=0`. The stall test must fail.
3. A **committed, renamed mapping mutant** implements the same two logical products using two explicit physical primitive instances with one live lane in each. Its separate map must report two DSP blocks and the one-DSP report parser must reject it. This proves the mapping detector fires on the fault it is meant to catch. The mutant is never in a production or correct-design source list.

The positive-control drivers pass only when the intended detector fires. They do not assert that a defect remains in correct RTL.

### 6.4 Exact MapOnly proof rows

For `dual18_explicit_pair`, all of the following are required:

- `Analysis & Synthesis Resource Usage Summary`: total DSP blocks = **1**;
- `Analysis & Synthesis Resource Utilization by Entity`: explicit wrapper hierarchy total = **1 DSP**, and top total = **1 DSP**;
- `Analysis & Synthesis DSP Block Usage Summary`: exactly one `Two Independent 18x18` row (or the tool's exact equivalent for mapped `m18x18_full`) and total number of DSP blocks = **1**;
- mapped hierarchy/source inspection: exactly one `cyclonev_mac` instance, with both `resulta` and `resultb` reaching distinct live outputs;
- no fixed-point/inferred multiplier outside that primitive and no lane implemented as ALUT multiplication.

For `dual18_s32x18_exact`, the same totals must be one, and the entity table must attribute the one DSP to `zhao_dual18_mul`; all shifted recombination is ALUT/carry logic. Record its ALUT/register estimate, but do not turn that MapOnly estimate into a production ALM or timing claim.

The inferred baseline is recorded as contrast. The two-primitive mutant must report two under the same report parser. Reports, effective QSF macros, source lists, tool build, device, and source digest form the receipt.

### 6.5 Pass, hold, and fail outcomes

- **PASS:** explicit pair = 1 DSP; exact signed32x18 = 1 DSP; both lanes independently pass generic/vendor differential and CE/reset tests; mapping and functional positive controls demonstrably fire. Production migration may begin, one subsystem packet at a time.
- **PHYSICAL PASS / MIGRATION HOLD:** map totals are one but no supported vendor functional simulator is available, signedness behavior is not differentially exercised, or a detector positive control has not fired. The physical lead remains interesting; no production RTL changes and no frontier subtraction are allowed.
- **FAIL:** explicit pair uses more than one block, either result is lost/merged, a lane goes to ALMs, `m18x18_full` is illegal on this tool/device, signed/mixed results differ, CE behavior differs, or the signed32x18 assembler uses more than one DSP. Strike projector 24-to-11 and every other explicit-packing target in this report. Return to a non-overlapping lever rather than fitting production variants of a dead premise.

## 7. Honest conditional DSP lattice

### 7.1 Evidence states

Use these states; never promote a number by prose:

- **I — implemented/functional, unfitted:** current prerequisite RTL exists but its frontier value is structural.
- **S — structurally possible, unmeasured:** arithmetic/site audit only. Every packed target in this report is S today.
- **M — MapOnly-confirmed:** the primitive discriminator passed. This proves resource shape only.
- **F — subsystem-fitted:** matched clean boundary fit proves DSP, ALM, M10K, and timing.
- **A — production-adopted:** the fitted implementation is in the meaningful current composition/manifest.

The 111 start contains mixed evidence and dependencies still at I. No row below is currently M, F, or A.

### 7.2 Non-overlapping priced-scope lattice

This is an arithmetic lattice, not a closure receipt. Each row replaces the immediately prior current charge and does not re-bank MATW=18, RCP v3, cull sequencing, pose sequencing, bake-v2 site sharing, or shared-projector deduplication already used to reach 111.

| conditional replacement | delta | priced-scope DSP | status now | caveat before subtraction becomes real |
|---|---:|---:|---|---|
| corrected legal structural frontier, RPP=3 | — | **111** | mixed/I | not a composed receipt |
| projector 24 -> 11 | -13 | **98** | S | primitive M, then projection/terrain F and A |
| current texture island 14 -> 9 | -5 | **93** | S | 14 itself is structural; one current island F and A |
| geometry LOD 6 -> 2 | -4 | **89** | S | independent operand-range proof and geometry F |
| terrain TESS selected 6 -> 1 | -5 | **84** | S | stale-cost attribution; T1 must establish matched delta |
| skin schoolbook 9 -> 6 | -3 | **81** | S | current three-lane workload and timing must pass geometry F |
| cull schoolbook 6 -> 4 | -2 | **79** | S | six is structural; same geometry F |
| terrain normals 3 -> 2 | -1 | **78** | S | current clean map has 3, but T1 timing is decisive |
| terrain LOD 3 -> 2 | -1 | **77** | S | T1 |
| pose mat3x4 3 -> 2 | -1 | **76** | S | geometry F; keep current sequenced form |
| bake-v2 conservative 6 -> 2 | -4 | **72** | S | 6 is an unfitted conservative frontier charge; T1 must measure actual delta |

The apparent 72 is 13 blocks below the owner's 85 target **inside this already-partial priced scope**. It is not comfortable whole-machine margin:

- the selected census still has 34 DSP-unpriced roots and 45 ALM-unpriced roots;
- `zhao_field_v3_mulbank` is DSP-unpriced and is therefore absent from both 192 and 111;
- if the complete FIELD composition requires that bank, schoolbook adds an omitted **+8** packed blocks (72 -> 80), while risky Karatsuba would add +6 (72 -> 78);
- other unpriced roots can add positive DSP;
- 30,000-ALM feasibility is unresolved; and
- several large deltas begin from stale or conservative charges, so matched fits may return less.

Optional Karatsuba is not in the preferred lattice. If later subsystem fits justify it, PERSPUV 4-to-3, skin 6-to-5, and cull 4-to-3 could conditionally lower the priced-scope point by three more blocks; FIELD could be 6 rather than 8. Those are ALM/timing trades, not free headroom.

### 7.3 What the lattice honestly says

The architecture can plausibly crack the physical 112 ceiling and can produce an arithmetic path below 85 in the priced scope. It does **not** presently demonstrate the owner's two-ceiling closure target, still less comfortable margin. If schoolbook wide decompositions fail ALM or timing, do not force the DSP arithmetic to 85 with Karatsuba. The next lever must be non-overlapping architectural removal—sharing or retiring a proved-live subsystem cost, or first pricing and redesigning an unpriced root—not another decomposition that spends unmeasured ALMs.

## 8. Required ending

### 1. Verdict

Explicit Cyclone V dual-18 packing is a **real, evidence-backed architecture candidate**. Quartus's 112 denominator is the physical variable-precision DSP-block limit; one physical block architecturally has two independent 18x18 lanes; and the cited inferred fit leaves the second lane unused in 22 independent-mode rows. The locally supported candidate is direct `cyclonev_mac` with `operation_mode="m18x18_full"`, not `altera_mult_add`. It remains unproved on Quartus 17 until the one-block map and vendor differential pass. No saving in this report is installed or measured.

### 2. Exact calibration-only first implementation packet

Create only these calibration artifacts; instantiate nothing in a production parent and change no workload, manifest, terrain module, shell module, or production fit target:

1. `fpga/rtl/common/zhao_dual18_mul.sv` — the new unreferenced combinational boundary, exact dual backend, four static signedness parameters, no state.
2. `tests/rtl/dual18_physical_pack_discriminator.sv` — identical registered shell and three separately selected tops: inferred pair, explicit pair, exact signed32x18.
3. `tests/dsp/dual18_physical_pack_directed.cpp` — independent widened oracle, exhaustive 8-bit classes, full-width boundaries/random vectors, reset/CE/stall/tag checks, and transcript comparison.
4. `tests/mutants/dual18_two_primitives_mutant.sv` — renamed two-physical-instance mapping positive control.
5. `tests/mutants/dual18_ce_ignore_mutant.sv` — renamed CE-hold positive control; the signedness/lane-swap control may be a deterministic test-harness injection because legal vectors reach it.
6. `tools/budget/gen_calib.py` — add four isolated MapOnly revisions targeting `5CSEBA6U23I7`: inferred, explicit, signed32x18, and two-primitive mutant; each emits its effective source list and macros.
7. `tools/budget/check_dual18_map.py` — parse the three named map-report tables, reject non-one totals for correct explicit variants, and prove rejection on the two-DSP mutant.

Quartus synthesis lists the wrapper/top and defines only `ZHAO_DUAL18_CYCLONEV`; Verilator lists the wrapper/top and defines only `ZHAO_DUAL18_BEHAVIORAL`; vendor simulation defines only the vendor macro and uses a Quartus-17-generated supported-simulator library setup. Run functional and positive-control gates first, then MapOnly. Do not fit a production leaf in this packet.

### 3. Exact MapOnly gate and outcomes

Gate name: **`dual18_physical_pack_discriminator`**.

- Pass only if `dual18_explicit_pair` reports total DSP = 1, wrapper hierarchy DSP = 1, one independent-18x18 mode row, one live `cyclonev_mac`, and two independently correct outputs; and `dual18_s32x18_exact` also reports total DSP = 1 with no helper DSP.
- Record inferred pair = 2 as expected contrast; inferred pair = 1 is useful good news and triggers reconsideration of whether the atom is needed, not failure.
- Prove the parser fires by rejecting the committed two-primitive mutant at DSP = 2. Prove functional and CE detectors fire on their controls.
- Hold all migration if vendor differential simulation is unavailable or any positive control has not fired.
- Fail and strike this architecture if the explicit correct variants use more than one DSP, lose/merge a lane, move one multiply to ALMs, reject the mode, or disagree bit-for-bit.

### 4. Production migration order after the gate passes

1. **Texture island packet:** pack RCP's already-decomposed partials, bilerp `pu0/pu1`, material-combine-v2's pair, and conservative schoolbook PERSPUV; run one texture-island-v3 subsystem fit. Do not start with PERSPUV Karatsuba.
2. **Projection/terrain composition packet:** migrate the nine signed32x18 row sites and the two factored viewport sites in the retained RPP=3/MATW=18 shared core; run the already-required meaningful composition fit before adoption.
3. **Terrain T1 packet:** batch exact schoolbook TESS, bake-v2, normals, and terrain-LOD changes; preserve every existing register/schedule and spend one T1 subsystem fit.
4. **Geometry packet:** batch range-proved geometry LOD, current sequenced mat3x4, two-lane cull, and three-lane skin schoolbook forms; run one geometry subsystem fit. Reject any result that worsens the already-dangerous skin timing or workload.
5. **FIELD packet:** price the current full FIELD composition first, then try schoolbook four-lane packing. Karatsuba is a later option only if the subsystem receipt shows enough ALM/Fmax margin for it.

Promote each delta M -> F -> A separately. Never carry a MapOnly count directly into the production frontier. Do not touch or infer savings from `zhao_shell_top.sv` or the separate D3 shell packet.

### 5. Remaining unknowns and explicitly unverified claims

- No Quartus command was run; no explicit primitive has mapped.
- `m18x18_full` is installed metadata, but the exact full parameter combination and two live outputs have not been accepted by Quartus 17 on this device.
- The encrypted primitive behavior has not been simulated; no installed ModelSim/Questa executable was found.
- The direct atom's signed, unsigned, mixed-sign, and signed-minimum behavior has not been differentially verified.
- The exact signed32x18 and viewport assemblers have not been implemented, simulated, or mapped.
- No added-ALM, register, routing, power, worst-path, or Fmax cost is measured for any packed form.
- Projector 24-to-11, texture 14-to-9, and every wide-product target are structural predictions only.
- The shared projector is functionally composed at the earlier composition commit but remains unadopted and unfitted; this report does not call its saving installed.
- Terrain bake v2 remains unfitted; TESS's six-DSP starting charge is mixed-age; cull's six is structural.
- The 111-DSP frontier is conditional and partial, not a current production total.
- FIELD's approximately 12-DSP unpacked/four-lane shape is not in the 192/111 bill; its packed target is an omitted positive charge until meaningful composition proves otherwise.
- Thirty-four roots remain DSP-unpriced and 45 remain ALM-unpriced. No whole-machine DSP or ALM closure, comfortable margin, or 30,000-ALM path is verified.
- M10K cannot be credited against new arithmetic ALM except where an actual behavior-preserving storage structure is separately shown to infer.
- Fog, forge, and shell savings are not banked because current meaningful composition or attribution is absent.
- Karatsuba savings are not in the preferred frontier and are explicitly unverified ALM/timing trades.
