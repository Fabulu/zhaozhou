# SuperStation hardware specification runner v1 — 2026-09-13

**Source commit:** `c68cc8ad`
**Quartus project:** `ZhaozhouSpecs`
**MiSTer core name:** `Zhaozhou Hardware Specs`
**Question:** Do selected committed Zhaozhou CRC, raster-fill, and packed-DSP
vectors execute correctly in the physical Cyclone V fabric behind the proven
SuperStation/MiSTer boundary?

## What executes

`fpga/rtl/platform/zhao_ssone_spec_tests.sv` sequences 16 checks after reset:

| Codes | Shipping RTL | Checks |
|---:|---|---|
| 1–6 | `zhao_crc32c_fold` | zero-byte identity; full 8-byte folds from two states/data words; 2-byte and 5-byte partial folds; illegal `n=9` returns the running state |
| 7–12 | `zhao_raster_fill` | negative reject; top-left zero accept; remainder-zero boundary accept/reject; positive accept; most-negative reject |
| 13–16 | `zhao_dual18_mul` | one preserved direct `cyclonev_mac`; lane A signed×signed and lane B unsigned×unsigned; zero, extrema, and lane-distinct asymmetric pairs |

The runner compares the two multiplier lanes independently on every multiplier
vector. It latches the first failing 1-based code and accumulates actual datapath
results into signature `e5f1c57f`. The HDMI presentation is:

- **green field / green signature bands:** all 16 comparisons passed and the
  accumulated signature matched;
- **red field / eight binary failure bands:** failed, with the first code;
- brief blue field: tests still running (normally below human perception).

`INJECT_FAILURE=1` changes only vector 1's expected result. The committed
Verilator bench instantiates both the shipping and injected versions and passes
only when shipping reports 16/16 with signature `e5f1c57f` and the detector
latches failure code 1. The runner also refuses output containing `%Fatal`; its
first version had printed PASS before falling through to a later fatal, which is
why that refusal is explicit.

## Synthesis gates

The separately named QSF:

- selects `ZHAO_DUAL18_CYCLONEV`, never the behavioral backend;
- uses the pinned `5CSEBA6U23I7` MiSTer device/pin/clock contract;
- keeps unused package pins input/tri-stated;
- leaves FPGA SDRAM, HPS-DDR requests, audio, SD request, UART, and user port
  inactive as in the proven probe.

Post-build verification must find all three shipping hierarchies plus
`cyclonev_mac` in the map report, at least 34 implemented DSPs (33 framework +
at least one new packed block), no critical warnings, no unused output-driving
pins, positive internal timing slacks, and an exact RBF receipt. A missing
hierarchy and missing 34th DSP each have a fired verifier control.

## Build result

The isolated full flow from exact source
`c68cc8ad3e480af6a8a0a5f3dcaffc81b7165d20` completed successfully in 4m35s:

- device/top: `5CSEBA6U23I7` / `sys_top`;
- 7,265 ALMs, 11,157 registers, 384,498 block-memory bits;
- 34 DSP blocks, 145 physical pins, zero virtual pins, three PLLs;
- zero errors; independent review later found one unnumbered/tabular Critical
  Warning that the original numbered-only parser missed (4-bit scaler mode into
  a 5-bit port);
- zero reserved output pins and 169 reserved input pins, but only USER_IO bits
  0/1/3/6 had permanently disabled output enables; physical SW[1] could make
  bits 2/4/5 drive MiSTer audio low;
- setup +0.648 ns, hold +0.250 ns, recovery +3.243 ns, removal
  +0.811 ns, minimum pulse width +1.122 ns;
- zero illegal and zero unconstrained clocks.

The post-map resource hierarchy—not merely source parsing—contains:

| Fitted node | ALUTs | Registers | DSPs |
|---|---:|---:|---:|
| `zhao_ssone_spec_tests:u_spec_tests` | 317 | 44 | 1 |
| `zhao_crc32c_fold:u_crc` | 133 | 0 | 0 |
| `zhao_raster_fill:u_fill` | 1 | 0 | 0 |
| `zhao_dual18_mul:u_mul` | 0 | 0 | 1 |

The direct multiplier owns `cyclonev_mac:u_dual18_mac`. Quartus 17 warns that
`preserve_hierarchy` is not a recognised synthesis attribute, so the audit does
not cite that attribute as evidence; it cites the actual post-map resource rows
and the 34th implemented DSP.

RBF: 2,425,124 bytes, SHA-256
`59407e97e208980c7965b931bcaf920291cadb40f56c623ec32cc3df672c86cd`.
Exact map, flow, pin, and timing reports plus all artifact hashes are preserved
with `HARDWARE-SPECS-BUILD-AUDIT.json`.

The timing non-claim remains: 4 input ports/14 paths and 50 output ports/122
paths have no board delays. This is not external-I/O timing closure.

## Physical result

The exact audited RBF loaded at `2026-09-13T17:51:11Z`. MiSTer reported
`Zhaozhou Hardware Specs`; FPGA manager remained `operating`; all three HPS/fabric
bridges and SSH remained live. After a 30-second hold, host rollback returned
MENU at `17:51:46Z`, disarmed the independently proven HPS watchdog, and removed
the staged RBF. The machine receipt is `status=ok`,
`rollbackSucceeded=true`, `stagedFileRemoved=true`, `error=null`.

At `2026-09-13T19:53:06+02:00` the owner reported **green signature bands** on
the physical display. Under the committed display contract, green requires all
16 independent comparisons to have left `fail_code_o=0` and the accumulated
actual-result signature to equal `e5f1c57f`. This closes the v1 selected-vector
question on the physical SuperStation One, subject to the claim boundary below.

### Independent review correction

The green result remains credible, but the historical build audit is now
`historical-invalidated` and the RBF cannot be loaded again. The original
verifier missed the tabular scaler-mode Critical Warning, and the MiSTer wrapper
could conditionally drive USER/SNAC bits 2/4/5 from physical SW[1]. Loader host
and rollback identity was also less strict than claimed.

Repair is build-copy-only so the pinned upstream tree remains exact: add the
fifth scaler-mode bit as a leading zero and force all seven USER_IO assignments
to high-Z. Future fit verification requires the patched build-copy digest, zero
Critical Warnings under an all-format scan, and all seven disabled USER_IO output
enables. Future loading additionally requires a V2 audit, complete source/build-
input/report/RBF/SOF manifest, pinned SSH/unit identity, exact FPGA/bridge/core
states, and complete watchdog fired/write/MENU evidence. None of those repaired
physical gates has run yet.

## Claim boundary

A physical green result may establish only:

1. the exact v1 selected vectors compared equal in this wrapper;
2. the preserved CRC/fill/multiplier hierarchy and one direct Cyclone V MAC were
   present in the fitted image, if the post-map gates pass;
3. reset-to-test-to-latched-display worked behind MiSTer.

It does **not** establish:

- exhaustive or random physical arithmetic correctness;
- mixed-sign dual18 modes beyond signed×signed lane A and unsigned×unsigned
  lane B;
- 32×18 recombination rails;
- multiplier enable/reset/pipeline sequencing (the wrapped primitive is
  deliberately combinational/stateless);
- independent host observation of both raw physical lane outputs;
- physical firing of lane-swap, lane-collapse, two-owner, or CE-ignore mutants;
- production shell migration, DSP savings, full rendering correctness, or game
  output;
- SDRAM, HPS fabric-DDR, audio, controller, GPIO/SNAC, JTAG, flash, persistent
  boot, or external-I/O timing.

Those are separate tests. Existing simulator/formal/random controls remain
valuable and must not be relabelled as silicon evidence. A later expanded
hardware suite needs host-readable per-lane results, mixed-sign and recombination
vectors, bounded random corpus, and separately fired physical controls before it
can close the broader dual18 question.
