# Dual-18 post-map atom-route evidence

Date: 2026-09-13

## Decision

The least expensive truthful proof that two logical 18x18 products share one Cyclone V variable-precision DSP is **MapOnly plus a Quartus Compiler Database atom-netlist query**. A fitter run is not required to prove mapped ownership and lane routing.

The implementation first passed that mapped-route question in a genuine but dirty diagnostic run, then repeated all four variants serially from clean pushed commit `65364dac5c83c8f6539f825846319b0c096bf9be`. The clean mapped-route packet is accepted for this narrow question. No production multiplier has migrated and no DSP saving is banked.

## Installed Quartus 17 capability and observed database shape

Quartus Prime Lite 17.0.2 Build 602 exposes the post-map database through `quartus_cdb` and `::quartus::atoms 1.0`:

- `C:/intelFPGA_lite/17.0/quartus/common/tcl/internal/init/atoms.advanced.hlp:19-32` documents Compiler Atom Netlist access from `quartus_cdb`.
- `C:/intelFPGA_lite/17.0/quartus/common/tcl/internal/init/atoms.cmds.advanced.hlp:7-47,70-88` documents `read_atom_netlist -type map` after successful `quartus_map`.
- The same reference documents `get_atom_nodes`, node name/type queries, typed ports, exact fanin/fanout, and optional Verilog export.

Genuine Quartus output corrected several earlier assumptions:

- the mapped `cyclonev_mac` owner node type is `MAC`, not `MAC_MULT`;
- its live operand ports are `AX/AY/BX/BY[0:17]`;
- its live logical results are `RESULTA/RESULTB[0:35]`;
- CDB also exposes unconnected physical-width tails `RESULTA[36:63]` and `RESULTB[36]`;
- top inputs enter through `IO_IBUF` node output port `O`, with names such as `ax_i[0]~input`;
- top outputs leave through `IO_OBUF` node input port `I`, with names such as `resulta_o[0]~output`;
- physical `IO_PAD` nodes are separate from those logical mapped boundaries.

The checker therefore ignores only unconnected physical-width tails and rejects any live out-of-range tail. It requires exact `IO_IBUF/O` and `IO_OBUF/I` boundaries rather than synthetic `PIN/PADIO` identities.

## Pre-named gate

Stage:

`dual18_postmap_lane_route_witness`

Implementation files:

- `tools/quartus/capture_dual18_atom_routes.tcl`
- `tools/budget/check_dual18_atom_routes.py`

Each invocation creates an exclusive short workspace below `<anchor-parent>/d18_runs/`, copies the immutable generated project there, runs canonical checker-owned `quartus_map`, snapshots the fresh map database, and only then runs canonical `quartus_cdb`. The full nonce remains in the external anchor and receipts; the short leaf is deterministically bound to the capture ID and a fresh 32-byte run token.

The CDB entry point uses the mapped database, not source elaboration:

```tcl
package require ::quartus::project
package require ::quartus::atoms 1.0
project_open -error_on_incompatible_database -revision $revision $project
read_atom_netlist -type map
```

The unversioned project-package request is deliberate. `quartus_cdb` has already loaded `::quartus::project 6.0`; requesting `2.0` produces a package-version conflict.

## Acceptance contract

All of these conditions are required:

1. The genuine map report has exactly one DSP block, one `Two Independent 18x18` mode row, and exactly two fixed-point logical multiplier rows.
2. The mapped graph contains exactly one `MAC` owner.
3. That owner has live exact `AX/AY/BX/BY[0:17]` and `RESULTA/RESULTB[0:35]` families.
4. Unconnected wider physical ports may exist, but any connected out-of-range family member rejects.
5. Every top input bit traverses exact CDB adjacency from its `IO_IBUF/O` boundary to only the corresponding owner operand bit.
6. Every owner result bit traverses exact CDB adjacency to only the corresponding `IO_OBUF/I` top output bit.
7. No invented internal atom arcs are permitted. An unexposed sequential or combinational internal arc yields **HOLD**.
8. The QPF/QSF, sources, preparation, fresh map logs/reports/summaries/database, CDB TSV/log/optional post-map Verilog, tool executables, timestamps, hashes, manifest, invocation nonce, and external anchor are bound in the receipt.
9. Synthetic fixtures permanently remain nonphysical and cannot satisfy the gate.

## Route-only top versus transaction testing

The original explicit top registered operands and results. Quartus 17 CDB does not expose arbitrary internal register D-to-Q data arcs, so exact top-boundary traversal stopped honestly at those registers.

The calibration now separates the questions:

- `dual18_explicit_pair` is a combinational route-only top for MapOnly+CDB;
- `dual18_explicit_pair_transaction` retains reset, CE, valid, tag, stall, and arithmetic behavior for Verilator.

This is not a bypass of transaction checking. The full direct behavioral corpus still exercises the registered transaction top, including backend-selection, CE-hold, and lane-swap positive controls. The mapped gate answers only ownership and route identity.

## Required genuine mapped controls

Every control runs through fresh canonical `quartus_map` plus CDB:

1. **Two-primitives mutant:** must expose two `MAC` owners and fire the one-owner detector.
2. **Lane-collapse mutant:** drives both logical outputs from RESULTA while retaining RESULTB on an independently observable wrong sink; must fire a result-origin detector.
3. **Lane-swap mutant:** crosses RESULTA and RESULTB while leaving operands unchanged; must fire a per-output origin detector.

The orchestration passes a control only when the genuine mapped graph produces the expected rejection.

## Troubleshooting chronology

None of these attempts is promotable:

| root | source state | result | use |
|---|---|---|---|
| original long clean-clone root at `fcdcd230` | clean pushed | Quartus internal paths reached 260/262 characters before CDB | failed path-boundary evidence only |
| `C:/d18-fd50a17e` | clean pushed `fd50a17e` | `COEFSELA` illegally connected while internal coefficients disabled | failed primitive-interface evidence only |
| `C:/d18-c15dac6b` | clean pushed `c15dac6b` | `AZ/BZ` connected with illegal zero-width operand-source configuration | failed primitive-interface evidence only |
| `C:/d18-diag1` | dirty diagnostic | Map passed; CDB rejected `::quartus::project` 2.0/6.0 conflict | troubleshooting only |
| `C:/d18-diag2` | dirty diagnostic | one DSP mapped; CDB owner was `MAC`, disproving assumed `MAC_MULT` | troubleshooting only |
| `C:/d18-diag3` | dirty diagnostic | real IO buffer identities observed; registered D-to-Q route remained unexposed | troubleshooting only |
| `C:/d18-diag4` | dirty diagnostic | unsupported input-port name query made connectivity unavailable | troubleshooting only |
| `C:/d18-diag5` | dirty diagnostic | route-only explicit gate passed and all three real controls fired | decisive diagnostic, still not promotable |

The primitive repair disables `az_width` and `bz_width` and leaves `AZ`, `BZ`, `COEFSELA`, and `COEFSELB` unconnected, matching Quartus 17's legal `m18x18_full` interface.

## Latest genuine diagnostic result

`C:/d18-diag5` was generated from the current dirty working tree under one external anchor:

```text
anchor SHA-256:   47caf02922cc8fd42261a4dc5d87d03f136a359df7172434be0865f282ff4395
invocation nonce: a54b9d92e0893aa5ac021e6e19fd321fc7d9fd07cf4d8558f19fc4c281440afc
manifest SHA-256: 21d2eca4b030de7761c4846096d2247928593d82ba817ef090d8856d6921a5a3
```

Results:

| variant | map result | CDB detector/result |
|---|---|---|
| explicit pair | 1 DSP; 2 fixed multipliers; 1 independent mode | **route gate PASS**; one `MAC`; 72 operand bits and 72 result bits checked |
| lane-collapse mutant | 1 DSP | detector fired: `resultb_o[0]` originated at RESULTA rather than RESULTB |
| lane-swap mutant | 1 DSP | detector fired: `resulta_o[0]` originated at RESULTB rather than RESULTA |
| two-primitives mutant | 2 DSPs | detector fired: expected one mapped `MAC`, got two |

All TSVs, optional post-map Verilog files, raw logs/reports/summaries, and map databases remain below that external diagnostic root. The explicit orchestration result is still overall `status=hold` because encrypted arithmetic semantics are separately unavailable. More importantly, the entire diagnostic set is dirty and may not be promoted.

## Clean pushed four-variant result

The route packet was regenerated after commit `65364dac5c83c8f6539f825846319b0c096bf9be` was pushed and independently read back at the same remote ref. The repository was clean throughout generation and all four serial invocations.

```text
external root:      C:/d18-65364dac
anchor SHA-256:     95f8f2b62d0968baa796d1e25382aae9c785d2c3202302eceb1c8b1b9680ed34
invocation nonce:   ccb9931896026edc404af185c8566e62272210f951f8ac673dc4b49e726cbfb0
manifest SHA-256:   1183b4242939f0fbd48a0276528419154546aa1d2fc8b62a03f269bb79cc8def
committed archive:  runs/CLAUDE-RUNS/RUN-20260912-1856-ceiling-architecture/dual18-map-cdb-65364dac.zip
archive SHA-256:    1d3d8354fe97673cbe38eafc053eaccf6b050d9353909a412b3a5b7c3d8ad859
```

Clean results:

| variant | map result | CDB detector/result |
|---|---|---|
| explicit pair | 1 DSP; 2 fixed multipliers; 1 independent mode | **route gate PASS**; one `MAC`; 72 operand bits and 72 result bits checked |
| lane-collapse mutant | 1 DSP | positive control PASS: `resultb_o[0]` origin mismatch fired |
| lane-swap mutant | 1 DSP | positive control PASS: `resulta_o[0]` origin mismatch fired |
| two-primitives mutant | 2 DSPs | positive control PASS: expected one mapped `MAC`, got two |

The 249-entry archive retains the anchor, manifest, all six generated configurations/QPF/QSF sets, four outer results, four fresh runtime projects, four map reports/summaries/logs, 132 map-database files, four CDB TSV/log/post-map sets, checker receipts, and a source-commit manifest. Its compact sidecar is `dual18-map-cdb-65364dac.receipt.json` in the same run folder.

This promotes only the named mapped-route question. The outer explicit status correctly remains `hold` because encrypted vendor arithmetic is unresolved. Production migration remains `none`, production saving remains zero, and the conditional resource frontier remains 111 DSP.

## Evidence boundaries

| evidence | permitted conclusion |
|---|---|
| MapOnly plus mapped CDB atom graph | one abstract mapped DSP resource owns two distinct live result-lane routes |
| tiny calibration fit plus `read_atom_netlist -type cmp` and one shared nonempty `DSP_X*_Y*_N0` location | final placement of both lanes at one physical site |
| encrypted vendor-model differential simulation | bit-exact signed/unsigned/mixed arithmetic and `AX*AY -> RESULTA`, `BX*BY -> RESULTB` semantics |
| production subsystem fit | ALM, timing, placement usability, and production resource saving |

If final placement is later required, repeat the atom query with `read_atom_netlist -type cmp`, require the same lane graph, and bind one identical nonempty DSP location plus the corresponding fitter detail row. That fit does not replace encrypted-model arithmetic validation, and neither calibration result replaces the eventual production fit.

## Encrypted-model availability

The official Cyclone V functional model chain is present, but the simulator needed to execute it is not installed:

- `C:/intelFPGA_lite/17.0/quartus/eda/sim_lib/cyclonev_atoms.v` declares `cyclonev_mac` and delegates behavior to `cyclonev_mac_encrypted`;
- `sim_lib/mentor/cyclonev_atoms_ncrypt.v` and `sim_lib/aldec/cyclonev_atoms_ncrypt.v` are protected Mentor and Aldec payloads;
- no compatible licensed ModelSim, Questa, or Aldec simulator is installed.

`quartus_sim.exe` is not a supported IEEE-1735 HDL decryptor and does not close this gate. The future differential must run the same UU, SS, SU, US, asymmetric, reset, CE, tag, and independent-lane corpus against both backends. Until then, arithmetic semantics remain **HOLD**.

## Current status

- Genuine dirty diagnostic mapped route: **PASS**, retained as troubleshooting only.
- Clean pushed four-variant mapped-route packet: **PASS** for abstract mapped ownership and lane routing.
- Three clean genuine mapped positive controls: **FIRED**.
- Final placement witness: **HOLD**.
- Encrypted vendor-model differential: **HOLD**.
- Production migration or DSP saving: **none**.

Official secondary references:

- https://docs.altera.com/r/docs/683236/25.3/quartus-prime-pro-edition-user-guide/generating-a-vqm-netlist-for-other-eda-tools
- https://docs.altera.com/r/docs/683375/current/cyclone-v-device-handbook-volume-1-device-interfaces-and-integration/variable-precision-dsp-blocks-in-cyclone-v-devices
