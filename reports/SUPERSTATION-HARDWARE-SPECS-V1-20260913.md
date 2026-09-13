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
