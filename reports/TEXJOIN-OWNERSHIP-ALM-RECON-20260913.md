# TEXJOIN ownership and ALM reconciliation

Date: 2026-09-13

## Decision

**Do not rearchitect `zhao_raster_texjoin_v2` yet. First prove whether it is a redundant accounting root.**

The strongest current standalone row is real and large: clean commit `8de11b1b...` fitted `zhao_raster_texjoin_v2` at **3,824 ALMs, 7,151 registers, 4 M10Ks, 0 DSP, and 93.12 MHz** (`reports/synthesis/zhao_block_fit.json:1206-1222`). Current RTL differs from that specimen only by comments, so it remains useful evidence about this module.

It is not yet evidence that 3,824 ALMs exist in one connected production circuit:

- the row is a standalone fit with 830 virtual pins;
- `fpga/rtl/prod/zhao_prod_top.sv:1-11` declares itself an unconnected resource-accounting hierarchy;
- the manifest selects TEXJOIN as a production root (`design/prod_manifest.yml:110-115`), but the only RTL instantiation found is that generated resource hierarchy (`fpga/rtl/prod/zhao_prod_top.sv:2943-3000`);
- the selected V3 texture island already owns fragment expansion/texture transaction state and emits final ordered fragments (`fpga/rtl/texture/zhao_texture_island_v3_top.sv:1-15,202-208`).

The immediate question is therefore ownership, not RAM inference. If the connected RASTER-to-TEXTURE design already has exactly one owner in V3, removing the standalone TEXJOIN root from the accounting manifest is the correct repair. Rebuilding unused RTL would spend engineering effort to optimize silicon that should not exist.

## Evidence classification

| observation | evidence class | permitted conclusion |
|---|---|---|
| TEXJOIN clean standalone fit: 3,824 ALM / 7,151 registers / 4 M10K / 0 DSP / 93.12 MHz | fitted leaf, current bytes except comments | this module is an expensive implementation |
| TEXJOIN appears in `prod_manifest.yml` and generated `zhao_prod_top` | current accounting structure | the selected mixed census charges it |
| no connected functional instantiation was found | source audit | likely accounting duplication; not yet a deletion receipt |
| V3 island already owns and emits ordered fragments | current functional source | strong ownership lead; exact seam still needs a composed proof |
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

Before changing `zhao_raster_texjoin_v2.sv`:

1. Freeze the connected RASTER-to-TEXTURE transaction boundary and enumerate every functional instance from accepted raster row to ordered textured fragment.
2. Prove exactly one owner allocates slots, issues TMU/AUX work, matches generation/token identity, and retires each legal fragment.
3. Compare the selected V3 path against TEXJOIN's legal-traffic contract for ordering, backpressure stability, and capacity. Separate sequence equivalence from cycle equivalence.
4. Require sustained one-beat-per-clock issue and retirement when work and downstream capacity permit; do not accept a test that ignores bubbles.
5. If V3 owns the boundary completely, remove the redundant TEXJOIN manifest root, regenerate the resource top, and require the census to remove exactly that selected row without changing connected RTL.
6. If a real consumer still requires the TEXJOIN interface, retain V2 unchanged as the oracle and only then implement a port-identical elastic RAM candidate beside it.

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
