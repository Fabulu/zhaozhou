# TEXJOIN ownership and ALM reconciliation

Date: 2026-09-13

## Decision

**Retire `zhao_raster_texjoin_v2` as a redundant selected accounting root, but do not describe V3 as connected console hardware or bank a physical ALM saving.**

The strongest current standalone row is real and large: clean commit `8de11b1b...` fitted `zhao_raster_texjoin_v2` at **3,824 ALMs, 7,151 registers, 4 M10Ks, 0 DSP, and 93.12 MHz** (`reports/synthesis/zhao_block_fit.json:1206-1222`). Current RTL differs from that specimen only by comments, so it remains useful evidence about this module.

It is not evidence that 3,824 ALMs exist in one connected production circuit:

- the row is a standalone fit with 830 virtual pins;
- `fpga/rtl/prod/zhao_prod_top.sv:1-11` declares itself an unconnected resource-accounting hierarchy;
- the manifest selects TEXJOIN as a production root (`design/prod_manifest.yml:110-115`), but the only RTL instantiation is that generated resource hierarchy (`fpga/rtl/prod/zhao_prod_top.sv:2943-3000`);
- V3 is a separate sibling instance in the same accounting hierarchy, not TEXJOIN's functional successor there;
- neither TEXJOIN nor `zhao_texture_island_v3_top` is instantiated by the current shell.

A source/elaboration ownership audit now settles the bookkeeping question: inside the selected V3 island, `zhao_texture_v3own` is the sole allocator, accepted-issue recorder, full-token return validator, ordered retire selector, and slot releaser. TEXJOIN has no functional consumer anywhere else. It may therefore move from manifest `top` to `excluded: superseded`, with its RTL, standalone target, oracle tests, and historical fit retained.

That result exposes a separate composition gap rather than closing one. The current shell feeds `zhao_geom_bin_pipe`/`zhao_raster_tile_pipe` flat texel fields supplied at the shell boundary; `zhao_raster_tile_pipe` explicitly contains no sampler. There is no shell Early-Z-to-V3 seam and no V3-to-`RASTER.FRAGMENT` seam. Consequently V3 is the sole owner **inside its selected subsystem**, but is not yet the live owner in the console hierarchy. Manifest retirement is an accounting correction only; it removes no demonstrated connected silicon and cannot support a physical ALM claim.

### Ownership chain found

The selected V3 island's live internal chain is:

1. admission and requirement derivation (`zhao_texture_island_v3_top.sv:590-621`);
2. synchronized reciprocal admission and owner allocation (`:662-689`);
3. generation-bearing descriptor capture, reciprocal/perspective transport, and exact descriptor rejoin (`:863-956,994-1056,1312-1357`);
4. TMU/AUX work creation with issue notification only on accepted handshakes (`:1364-1401`);
5. the sole lifecycle owner `zhao_texture_v3own:u_own` (`:1477-1523`), which allocates/stamps owners, records issued work, validates terminal returns, selects allocation-order output, and releases only on output acceptance;
6. opaque-handle TMU and AUX execution/return, metadata identity join, material combine, and final owner return (`:1776-1791,1896-1910,1993-2039,2527-2711,2920-2954,3054-3103,3399-3554`).

Those surrounding blocks transport, calculate, store, or independently check identity; none owns fragment lifetime. In the generated census hierarchy, TEXJOIN and V3 instead receive separate private stimulus and are sibling instances (`zhao_prod_top.sv:2882-3000,4357-4525`).

## Evidence classification

| observation | evidence class | permitted conclusion |
|---|---|---|
| TEXJOIN clean standalone fit: 3,824 ALM / 7,151 registers / 4 M10K / 0 DSP / 93.12 MHz | fitted leaf, current bytes except comments | this module is an expensive implementation |
| TEXJOIN appears in `prod_manifest.yml` and generated `zhao_prod_top` | current accounting structure | the selected mixed census charges it |
| no functional TEXJOIN instantiation exists; its only instance is the generated accounting top | current source/elaboration audit | retire its selected root as an accounting correction |
| V3's `zhao_texture_v3own` alone owns allocation, accepted issues, token validation, ordered retirement, and release inside that island | current subsystem ownership audit | V3 has exactly one internal owner; TEXJOIN is not part of it |
| neither V3 nor TEXJOIN is instantiated by `zhao_shell_top`; the shell accepts flat texels externally | current connected-shell audit | V3 is not yet live console hardware; a shell-to-texture composition seam remains open |
| subtracting 3,824 from 58,359 | arithmetic on a mixed partial census | never a current composed ALM saving |

If ownership is confirmed and the redundant root is retired, the selected accounting census will decrease by the retired row. That is an **accounting correction**, not proof that a physically connected design shrank: an uninstantiated root consumed no silicon in that design. Only the eventual composed boundary fit can establish the physical total.

## Why a direct memory rewrite is second, not first

At default depth 16, TEXJOIN declares 7,056 entry bits (`fpga/rtl/raster/zhao_raster_texjoin_v2.sv:156-173`):

| family | bits per slot | total bits |
|---|---:|---:|
| control | 21 | 336 |
| three descriptors | 228 | 3,648 |
| three primary results | 96 | 1,536 |
| context | 64 | 1,024 |
| AUX result | 32 | 512 |
| **total** | **441** | **7,056** |

The fitted 7,151-register total is not a bit-for-bit attribution of this table. It also includes queues, held packets, pointers, and telemetry; samples 1 and 2 have result arrays but no observable retirement reader (`zhao_raster_texjoin_v2.sv:330-341`). The fit archive lacks a named TEXJOIN RAM summary for assigning the four M10Ks.

The storage pressure is still credible. Relevant structural causes are:

1. the 336 control bits are reset and independently updated, so they belong in flops;
2. descriptors are multidimensional unpacked arrays with dynamic sample selection, while the project RAM checker prefers a flat row or static banks (`tools/quartus/check_ram_inference.py:52-62`);
3. result banks combine multidimensional shape, dynamic bank selection, a dynamic retirement read, and two write addresses for sample 0 (`zhao_raster_texjoin_v2.sv:415-418,466-470`);
4. context needs one write and two simultaneous reads, requiring replication for RAM;
5. AUX result is ordinary 1W1R payload.

The old claim that an asynchronous-reset process alone prevents M10K inference is too broad. `tools/quartus/check_ram_inference.py:23-45` retains counterexamples where unreset arrays infer despite the enclosing process. Port shape, read timing, representation, and whether the array itself resets are the actual questions.

## Ownership-first no-Quartus packet

The source/elaboration audit completed the first ownership pass: TEXJOIN is accounting-only, V3 has one internal lifecycle owner, and the current shell has no texture-island seam. The implementation packet should now:

1. Record the ownership role explicitly and add a role-aware exactly-one-owner checker; graph containment alone cannot distinguish two disconnected implementations of the same logical role.
2. Keep `zhao_raster_texjoin_v2` unchanged as the retained behavioral oracle.
3. Move TEXJOIN from manifest `top` to `excluded: superseded`, regenerate `zhao_prod_top.sv`, and prove the generated census removes only its private stimulus/instance/fold branch.
4. Preserve its standalone fit target, directed/differential tests, and historical evidence; optionally remove only its unused production compile-pool entry.
5. Retain V3's response controls and add the independent full-identity stall and uninterrupted-handshake controls below before calling the accounting packet evidence-complete.
6. Treat the absent shell-to-V3 seam as a separate production-composition blocker. Do not imply that manifest selection installs V3 in the shell.
7. If a future real consumer still requires the TEXJOIN interface, retain V2 as the oracle and only then implement a port-identical elastic RAM candidate beside it.

### Required controls

- **Ownership duplication control:** a test-only composition with both owners enabled must fail the exactly-one-owner checker.
- **Stall identity control:** independently scoreboard the full accepted descriptor by `{slot, generation, sample}`; a renamed slot-swap mutant must fire even when aggregate counters balance.
- **Bubble control:** preload work, release ready, and require uninterrupted handshakes; a renamed no-same-edge-reload mutant must fail.
- **Response controls:** exercise wrong generation, index 3, unrequested, not-issued, duplicate, and simultaneous bad TMU/AUX returns. Explicitly choose legacy behavior or the safer six-term refusal law rather than inheriting it accidentally.
- **Overflow control:** because legal stimulus cannot exceed a correct work queue, retain a renamed committed small-queue mutant whose inverse-polarity driver passes only when overflow fires.
- **RAM-shape controls if a rewrite proceeds:** prove the inference checker rejects a combinational-read variant and a two-dynamic-write-address variant, not merely that the intended source looks RAM-like.

## Conditional RAM candidate

Only if ownership proves a real consumer needs the V2 interface, the preferred candidate is a port-identical elastic implementation:

- keep 336 control bits in flops;
- use one packed 16x228 descriptor row (six 256x40 M10Ks);
- use three static 16x32 result banks (three M10Ks), or one only if the contract intentionally freezes sample-0-only behavior;
- duplicate 16x64 context for AUX and retirement (four M10Ks);
- use one 16x32 AUX-result bank (one M10K);
- keep the small multiwrite work queue in logic;
- read into existing held packet registers with clock enables, not three-state read FSMs;
- preserve one accepted/issued/retired beat per clock and every backpressure hold law.

The all-sample shape is approximately 14 M10Ks; strict sample-0 behavior approximately 12. The old clean FRAGROB fit at 1,676 ALMs / 2,631 registers / 13 M10Ks / 103.1 MHz (`zhao_block_fit.json:1727-1746`) supports only the **direction** of a possible 1,500-2,100-ALM recovery. Its RTL is stale, its current map-only successor is dirty, and its three-cycle issue/retirement FSM cadence is not acceptable (`zhao_texture_fragrob.sv:369-395,756-781,838-860`). It is not an adoption candidate as-is.

## Named subsystem gate

**`g8a_raster_texture_single_owner_characterization`**

Question:

> With the selected V3 texture island connected to the downstream raster-fragment path, does exactly one transaction owner remain, sustain one accepted and retired fragment per clock wherever downstream capacity permits, keep DSP unchanged, and produce a clean composed ALM/register/M10K receipt with no redundant TEXJOIN root?

This is a same-source composed subsystem fit after every simulation and mutant gate passes. It is not another TEXJOIN leaf fit and not an unconnected `zhao_prod_top` timing run. Until it lands:

- do not bank 3,824 ALMs as a physical saving;
- do not rewrite TEXJOIN storage;
- do not call the selected mixed census a connected production total;
- do not spend a Quartus run on the ownership question, which source/elaboration and simulation must answer first.

## Programme consequence

D3 shell attribution remains the immediate measurement boundary. Its compile pool may contain a source without that source being live in the elaborated shell; compile-pool membership is not ownership evidence. Once Packet A, terrain integration, and Packet B establish a truthful shell hierarchy, the first ALM action should compare:

1. retirement of any proved redundant accounting root such as TEXJOIN;
2. a measured leading live shell owner;
3. only then a storage or spatial-logic rearchitecture.

This keeps the 30,000-ALM target tied to connected silicon rather than optimizing and summing mutually exclusive leaves.
