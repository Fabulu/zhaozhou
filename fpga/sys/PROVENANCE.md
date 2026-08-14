# sys/ provenance (placeholder — directory intentionally empty)

The MiSTer framework `sys/` directory (~55 files: sys_top.v, sys.tcl,
hps_io.sv, audio, scaler, PLL IPs, ...) is **vendored verbatim** from
[MiSTer-devel/Template_MiSTer](https://github.com/MiSTer-devel/Template_MiSTer)
when the hardware lane opens. It is *prohibited to change* upstream files
(template rule: framework updates erase customizations).

**Pin strategy (charter 29-3, "never vendor without a pin"):**

- vendor at an exact upstream commit hash;
- record the hash + tree SHA in this file;
- CI check: `git diff` the vendored tree against the pinned hash.

**Current state:** BLOCKED-on-hardware (ZH-000, ledger SYS.* blocks,
`blocked_on: hardware`). The Verilator lane needs only `fpga/rtl/` and never
imports sys/. Nothing in wave 1 depends on this directory existing beyond
this provenance note.
