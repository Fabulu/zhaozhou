# Dual-18 post-map atom-route evidence

Date: 2026-09-13

## Decision

The least expensive truthful proof of two logical 18x18 products sharing one Cyclone V variable-precision DSP is **MapOnly plus a Quartus Compiler Database atom-netlist query**. A fitter run is not required to prove mapped lane ownership.

The current calibration remains **HOLD**. No Quartus command has been run, no genuine atom-database witness exists, no production multiplier has migrated, and no DSP saving is banked.

## Installed Quartus 17 capability

Quartus Prime Lite 17.0.2 Build 602 exposes the required post-map database through `quartus_cdb` and `::quartus::atoms 1.0`:

- `C:/intelFPGA_lite/17.0/quartus/common/tcl/internal/init/atoms.advanced.hlp:19-32` documents Compiler Atom Netlist access from `quartus_cdb`.
- `C:/intelFPGA_lite/17.0/quartus/common/tcl/internal/init/atoms.cmds.advanced.hlp:7-47,70-88` documents `read_atom_netlist -type map` after successful `quartus_map`.
- The same command reference documents `get_atom_nodes` at lines 330-420, `get_atom_node_info -key NAME|TYPE` at 631-721, `get_atom_oport_by_type` at 975-1054, `get_atom_port_info -key fanout|type|literal_index` at 1194-1304, and `write_atom_netlist -verilog -file ...` at 96-200.
- The installed atom database exposes the exact port-type names `AX`, `AY`, `BX`, `BY`, `RESULTA`, and `RESULTB`; installed package documentation names `MAC_MULT` as a legal basic atom type.

These APIs can inspect the genuine mapped graph. A generated Verilog/VQM dump may be retained for audit, but parsing its text is not route proof.

## Pre-named gate

Stage:

`dual18_postmap_lane_route_witness`

Proposed implementation files:

- `tools/quartus/capture_dual18_atom_routes.tcl`
- `tools/budget/check_dual18_atom_routes.py`

For each existing content-addressed calibration revision, run from its generated project directory:

```text
C:/intelFPGA_lite/17.0/quartus/bin64/quartus_map.exe <revision>
C:/intelFPGA_lite/17.0/quartus/bin64/quartus_cdb.exe \
  -t <repo>/tools/quartus/capture_dual18_atom_routes.tcl \
  <project> <revision> \
  output_files/<revision>.dual18.atom.tsv \
  output_files/<revision>.post_map.vo
```

The Tcl entry point must use the mapped database, not source elaboration:

```tcl
package require ::quartus::project 2.0
package require ::quartus::atoms 1.0
project_open -revision $revision $project
read_atom_netlist -type map
```

The final checker should invoke the canonical `quartus_cdb.exe` itself in a fresh anchored output directory. It must reject pre-existing or caller-supplied route artifacts.

## Acceptance contract

All of these conditions are required:

1. Existing report gates show exactly one DSP block, exactly one `Two Independent 18x18` mode row, and exactly two logical multipliers.
2. The genuine post-map atom database contains exactly one mapped atom node owning both `RESULTA` and `RESULTB`.
3. Every `RESULTA[0:35]` and `RESULTB[0:35]` port exists.
4. The 72 result-port identities are pairwise lane-correct and each port has live mapped fanout.
5. Graph traversal proves, for every bit, that logical `resulta_o[i]` originates only at that atom's `RESULTA[i]` and logical `resultb_o[i]` originates only at its `RESULTB[i]`.
6. The same mapped atom owns the expected `AX/AY/BX/BY[0:17]` operand-port families, and their graph cones agree with the four top operands.
7. The TSV, optional atom Verilog dump, CDB stdout/stderr, Quartus version, canonical paths, timestamps, and hashes are bound into the existing content-addressed revision, manifest, and external invocation anchor.
8. Missing, encrypted, ambiguous, or unavailable atom connectivity yields **HOLD**. Source regexes, inferred source wiring, ordinary `.map.rpt` text, and synthetic fixtures may not substitute for mapped graph evidence.

## Required real controls

Every detector control must run through genuine `quartus_map` plus the CDB atom query. Parser fixtures test mechanics only.

1. **Two-primitives control.** Map committed `tests/mutants/dual18_two_primitives_mutant.sv`; the atom query must expose two DSP owners and the one-block gate must reject it.
2. **Lane-collapse control.** Add a renamed committed mutant that drives both logical outputs from `RESULTA` and leaves `RESULTB` dead. The mapped origin/live-port checks must fire.
3. **Lane-swap control.** Add a renamed committed mutant that crosses the two result lanes while leaving operands unchanged. The per-logical-output origin checks must fire.

The control driver passes only when each real map/CDB run produces the expected rejection. It must not accept hand-authored atom fixtures as physical evidence.

## Evidence boundaries

| evidence | permitted conclusion |
|---|---|
| MapOnly plus mapped CDB atom graph | one abstract mapped DSP resource owns two distinct live result-lane routes |
| tiny calibration fit plus `read_atom_netlist -type cmp` and one shared nonempty `DSP_X*_Y*_N0` location | final placement of both lanes at one physical site |
| encrypted vendor-model differential simulation | bit-exact signed/unsigned/mixed arithmetic and `AX*AY -> RESULTA`, `BX*BY -> RESULTB` semantics |
| production subsystem fit | ALM, timing, placement usability, and production resource saving |

If final placement is later required, repeat the atom query with `read_atom_netlist -type cmp`, require the same lane graph, and bind one identical nonempty DSP location plus the corresponding fitter detail row. That fit does not replace encrypted-model arithmetic validation, and neither calibration result replaces the eventual production fit.

## Current status

- Quartus capability path: **verified from the installed 17.0.2 API and database**.
- Genuine mapped route witness: **HOLD**.
- Final placement witness: **HOLD**.
- Encrypted vendor-model differential simulation: **HOLD**.
- Production migration or DSP saving: **none**.

Official secondary references:

- https://docs.altera.com/r/docs/683236/25.3/quartus-prime-pro-edition-user-guide/generating-a-vqm-netlist-for-other-eda-tools
- https://docs.altera.com/r/docs/683375/current/cyclone-v-device-handbook-volume-1-device-interfaces-and-integration/variable-precision-dsp-blocks-in-cyclone-v-devices
