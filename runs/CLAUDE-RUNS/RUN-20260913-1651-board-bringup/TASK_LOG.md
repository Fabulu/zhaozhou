# Task Log: RUN-20260913-1651 - SuperStation FPGA board bring-up

**Created:** 2026-09-13 16:51 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/

---

## Objective

Identify the connected SuperStation board and establish a documented, reversible
safety gate for eventual minimal FPGA bring-up and SSH access. Discovery remains
read-only until board device, voltages, clock/reset, pinout, and design safety are
verified.

---

## Progress Timeline

### 2026-09-13 16:51 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260913-1651.
- Created dedicated checkout `zhaozhou-board-bringup-20260913`.
- Created branch `zhaozhou-board-bringup-20260913` from the latest pushed
  hardware branch `origin/claude/ceiling-architecture-20260912` at
  `158442b57b32f9bbf9cc9e241a88f43c2a660f7a`.
- Confirmed this lane does not own Texture R0 Packet B or the pinned 23:00 shell
  fit; those remain in the separate hardware-shell-fit session.
- Started with a clean checkout; no shared build directory was selected.

### 2026-09-13 17:29 UTC+02:00 - Read-only board and network detection

- `jtagconfig.exe -n` returned `No JTAG hardware available`; Windows PnP also
  shows no present USB-Blaster/JTAG device.
- Router DNS identified `mister.fritz.box` at `192.168.178.59`; TCP 22 exposes
  OpenSSH 8.6. Password SSH with the standard MiSTer root account succeeded.
- Captured SSH host-key fingerprints and the live Ethernet identity.
- Live device tree reports Cyclone V SoC compatibility, generic
  `Terasic DE10-nano` model text, and 25 MHz HPS `osc1`; the generic model is a
  compatibility DT and not physical-PCB proof.
- FPGA manager reports `operating`; no bitstream was loaded by this run.
- Device tree declares a 1 GiB HPS DDR address map; boot arguments expose 511
  MiB to Linux and reserve 513 MiB. The exact DRAM component marking is open.
- Live USB topology shows Wi-Fi/Bluetooth, IR, NVMe, and optical-drive paths,
  strongly indicating the SuperDock is attached.
- Pinned the canonical MiSTer target at commit
  `3ea1134cf05d62c2b1db30362277a823d739ced2`: `5CSEBA6U23I7`, UFBGA-672,
  speed grade 7; three 50 MHz FPGA inputs; explicit soft I/O at 3.3-V LVTTL.
- Found no public SuperStation motherboard schematic, BOM, Gerbers, netlist, or
  revision-specific electrical pinout. The complete MiSTer `sys.tcl` is the
  reproducible FPGA-facing compatibility pinout, not a board schematic.
- Owner supplied the exterior identification: Retro Remake Hong Kong Limited
  SuperStation One, model marking `RCSH-1001/1002`, rated `5 V / 9 V DC` and
  `EXT: 5 V 3 A / 9 V 3 A`. This closes product identity and rated input, but
  not live USB-PD negotiation, PCB revision, or FPGA top marking.
- Did not ask the owner to open a powered unit or remove a heatsink.
- Wrote `reports/BOARD-BRINGUP-SUPERSTATION-ONE-20260913.md` with confirmed,
  published, inferred, and open facts separated.

### 2026-09-13 17:41 UTC+02:00 - Software, core, storage, and recovery receipt

- Identified the live `/media/fat/MiSTer` byte-for-byte as upstream
  `MiSTer_20260912`; did not trust the deliberately spoofed `/MiSTer.version`
  marker.
- Identified the then-active `SNES_20260823.rbf` byte-for-byte against the
  upstream release, proving an unmodified MiSTer artifact configures and runs
  on this physical unit.
- Confirmed FPGA manager `operating` and all three HPS/fabric bridges enabled.
- Captured the boot image hash, storage layout/free space, selected MiSTer INI
  settings, recovery files, and backup inventory without changing SD contents.
- Confirmed `/dev/MiSTer_cmd` is a FIFO and pinned the upstream implementation of
  `load_core <path>`; recorded `menu.rbf` and its hash as the volatile rollback.
- Owner explicitly directed the lane to proceed. Narrowed the remaining block:
  MiSTer-managed volatile RBF loading may proceed after a clean minimal-build
  pin audit and a rehearsed rollback; raw JTAG, configuration flash, and
  non-MiSTer board-specific pin use remain blocked.

### 2026-09-13 18:02 UTC+02:00 - Reproducible MiSTer source and safe core

- Vendored all 57 files in the pinned Template_MiSTer `sys` tree at commit
  `3ea1134cf05d62c2b1db30362277a823d739ced2`; reconstructed tree ID
  `9f95eddd65ebfca9b8dd94ed1a48e3f867165aa8` exactly.
- Preserved the upstream mixture of LF, CRLF, and mixed-ending generated IP with
  a path-specific `-text` rule; a first normalization-based checker correctly
  failed and was replaced rather than weakening the expected digest.
- Added the dedicated `ZhaozhouBringup` Quartus project and safe `emu` core:
  HPS bridge, 50 MHz clock/reset, color bars, and LED heartbeat are intentional;
  SDRAM, HPS-DDR requests, SD, UART, user port, and audio are inactive.
- Explicitly set all unused package pins to `AS INPUT TRI-STATED`.
- Added an isolated build script that requires the exact checkout/branch, clean
  committed source, no competing Quartus process, and its own marked build
  directory. Fired the dirty-source guard deliberately before committing.
- Added the source/vendor/pin-policy verifier and six positive-control tests;
  all six pass, including mutations of device, vendor tree, EOL protection,
  unused-pin policy, and SDRAM tri-state.
- Owner reported the console rebooted and available for the upcoming volatile
  load. No board state or SD contents were changed in this step.

---

## Subagent Spawns

None. This task is restricted to the dedicated Claude Code hardware session.

---

## Files Created

- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/TASK_LOG.md`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/SPEC_v1.md`
- `reports/BOARD-BRINGUP-SUPERSTATION-ONE-20260913.md`
- `fpga/sys/` — 57 byte-pinned upstream framework files plus license/provenance
- `fpga/ZhaozhouBringup.qpf`
- `fpga/ZhaozhouBringup.qsf`
- `fpga/ZhaozhouBringup.sdc`
- `fpga/files_bringup.qip`
- `fpga/rtl/platform/zhao_ssone_bringup.sv`
- `tools/board/build_superstation_bringup.ps1`
- `tools/board/verify_superstation_bringup.py`
- `tests/tools/test_verify_superstation_bringup.py`

---

## Decisions Made

- Treat the connected SuperStation hardware as the current specification-test
  target, distinct from the later reference/debug FPGA board.
- Read-only USB/JTAG/network discovery is permitted now.
- The owner explicitly authorised proceeding on 2026-09-13. Volatile loading
  through the pinned MiSTer/HPS contract is permitted after the minimal RBF's
  final pin audit and rollback rehearsal. JTAG, configuration flash, and pins
  outside that contract remain blocked.

---

## Next Steps

1. Commit and push the expanded read-only receipt.
2. Add the isolated, pinned MiSTer wrapper and minimal safe core without touching
   Packet-B or shared build state.
3. Compile in a new checkout-local board build directory and audit final device,
   clocks, I/O standards, pin locations, unused-pin policy, and warnings.
4. Rehearse the SSH `menu.rbf` rollback command, then perform the first volatile
   minimal load and immediate rollback.
5. If the PCB later becomes safely accessible for another reason, capture its
   silk revision and FPGA top marking without opening a powered unit.
