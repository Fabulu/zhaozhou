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

---

## Subagent Spawns

None. This task is restricted to the dedicated Claude Code hardware session.

---

## Files Created

- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/TASK_LOG.md`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/SPEC_v1.md`
- `reports/BOARD-BRINGUP-SUPERSTATION-ONE-20260913.md`

---

## Decisions Made

- Treat the connected SuperStation hardware as the current specification-test
  target, distinct from the later reference/debug FPGA board.
- Read-only USB/JTAG/network discovery is permitted now.
- No programming or FPGA pin drive until the complete safety gate is verified.

---

## Next Steps

1. Commit and push the identification report and RUN on the dedicated branch.
2. Design a build-only, explicitly tri-stated MiSTer-compatible probe and audit
   its final Quartus pin report; do not load it while the safety gate is open.
3. If the PCB later becomes safely accessible for another reason, capture its
   silk revision and FPGA top marking without opening a powered unit.
