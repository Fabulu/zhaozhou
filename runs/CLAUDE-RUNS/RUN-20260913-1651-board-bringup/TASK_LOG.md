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

### 2026-09-13 18:05 UTC+02:00 - First board compile started

- Committed and pushed the exact build source as `4d74f0fc` before compiling.
- Started `build_superstation_bringup.ps1` in the background. It copied its
  closure into checkout-local ignored `build-board-superstation/`; no shared
  build directory is involved and source files are no longer live inputs.
- **Work in progress while fit runs:** implement the guarded SSH stage/load/
  observe/rollback transaction and its tests outside the copied fit closure.
- **Next step:** finish that transaction tool, then record where it stands before
  reading the Quartus result and performing the final pin audit.

### 2026-09-13 18:06 UTC+02:00 - Build result arrived; work parked first

- The background build reported failure.
- **Parked work:** the guarded load/rollback transaction design had established
  `/dev/MiSTer_cmd` for loading and `/tmp/CORENAME` plus `/tmp/RBFNAME` for live
  identity. No load script file had yet been authored.
- **Resume after attending the build:** diagnose and repair the isolated compile,
  commit the repair, rebuild cleanly, then return to the transaction tool with
  those three runtime interfaces.

### 2026-09-13 18:13 UTC+02:00 - First compile failure repaired

- The compile failed in 74 seconds during Analysis & Synthesis; no fitter or
  assembler ran and no RBF was created.
- Root cause was concrete: the canonical framework's `sys/pll_q17.qip` expected
  `rtl/pll.qip`, and the core drove `CLK_VIDEO` directly from a package clock.
  Quartus reported missing `rtl/pll.qip` and rejected that direct clock on
  clock-control `inclk[3]` for both HDMI and VGA.
- Imported the four exact pinned Template_MiSTer core-PLL blobs, recorded their
  Git object IDs, preserved them with `-text`, and extended the verifier.
- Changed the core to the template's 50 MHz -> 20 MHz PLL, reset-gated on
  `locked`, and the template's 638x262 raster with a 10 MHz pixel enable
  (approximately 59.9 frames/s).
- The checker now has seven fired positive controls, including a PLL mutation;
  all seven pass and both the framework tree and four PLL blobs match upstream.
- No board or SD state changed. The failed build remains only in the owned,
  ignored checkout-local build directory and will be replaced with `-Clean`
  after this repair is committed.

### 2026-09-13 18:19 UTC+02:00 - Guarded live transaction prepared

- Committed/pushed the PLL repair as `a28769a1` and started one clean rebuild in
  the isolated build directory.
- Added `invoke_superstation_probe.ps1` outside the running fit closure. It
  supports a menu rollback rehearsal and an automatic probe transaction.
- The transaction requires explicit `-Execute`, exact checkout/branch, strict
  known-host checking, an owned build marker, unchanged RBF source paths, a
  passing source/build audit, an unused remote staging name, and byte-identical
  local/remote SHA-256 before loading.
- Probe mode records before/load/rollback FPGA-manager, bridge, core, RBF and
  MiSTer-process state; always attempts `menu.rbf` rollback after a load; and
  removes only the uniquely named file it created after successful rollback.
- Fired the no-`-Execute` guard deliberately; it refused before any SSH or board
  action. PowerShell syntax parsing passes.
- **Work while fit continues:** transaction tool is authored; next is commit it,
  then inspect the single running build when its result arrives. No second
  Quartus process and no hardware load will start meanwhile.

### 2026-09-13 18:28 UTC+02:00 - Second build result arrived; work parked first

- The repaired background build reported failure; all board-lane Quartus
  processes have exited.
- **Parked work:** `invoke_superstation_probe.ps1` is authored and parse-clean;
  its explicit execution guard fired. It is not yet committed, and no hardware
  command has been issued.
- **Resume after attending the build:** preserve and inspect the exact compile
  reports, state the device/pin/timing/non-claim boundary, repair only the
  demonstrated cause, then return to the load tool.
- The owner subsequently removed the witness hold: proceed and preserve a
  replayable receipt so the test can be run again when they are watching.

### 2026-09-13 18:37 UTC+02:00 - Quartus success and final audit

- The second Quartus flow itself was successful: Analysis & Synthesis, Fitter,
  Assembler, and TimeQuest all completed in 5m28s with zero errors.
- The job's failure status came only from the post-build Python checker decoding
  Quartus's Windows-ANSI degree symbol as UTF-8. Fixed the reader to accept the
  report's actual encoding and audited the existing build; no Quartus rerun was
  needed.
- Final audit passes: `5CSEBA6U23I7`, `sys_top`, three canonical 3.3-V 50 MHz
  clock pins, zero virtual pins, zero critical warnings, zero unused
  output-driving pins, and 169 reserved input pins.
- Timing is positive: setup +0.217 ns, hold +0.246 ns, recovery +4.141 ns,
  removal +0.859 ns, minimum pulse width +1.122 ns; zero illegal or
  unconstrained clocks.
- Non-claim remains explicit: 4 input and 50 output ports lack board-delay
  constraints, so external I/O timing is not signed off by this fit.
- RBF: 2,429,104 bytes,
  SHA-256 `7e7b46f79685383dc85a057f88602154039e27cf4bea37fdfb911a55948ea9c0`.
- Preserved exact flow, pin, and timing reports plus hashes for map/fit/asm/RBF/
  SOF in `QUARTUS-BUILD-AUDIT.json`.
- Strengthened the transaction with a verified `menu.rbf` hash and an HPS-side
  rollback watchdog that is armed before the candidate load and is independent
  of FPGA fabric/video/bridge behavior.

### 2026-09-13 18:43 UTC+02:00 - Rollback rehearsed; candidate not yet loaded

- Committed/pushed the audit and guarded transaction tool as `d6dec802`.
- Rehearsed the known menu rollback: `menu.rbf` matched its expected SHA-256;
  MENU returned; SSH stayed reachable; FPGA manager stayed `operating`; all
  three bridges stayed enabled. MiSTer restarted with a new PID.
- The first candidate transaction staged and byte-verified the RBF, then stopped
  **before `load_core`** while arming the watchdog: PowerShell's case-insensitive
  `$PID` built-in cannot be assigned through local `$pid`.
- Receipt proves `loaded: []`, `rollbackAttempted: false`, board remained MENU,
  and the staged RBF was removed. Preserved it as
  `FIRST-VOLATILE-LOAD-ATTEMPT1-NOLOAD.json`; do not call this a hardware test.
- Renamed the local watchdog variable to `$watchdogPid`; no RTL, RBF, or board
  state changed by the repair.

### 2026-09-13 18:46 UTC+02:00 - FIRST Zhaozhou RBF ran on hardware

- Committed/pushed the watchdog repair as `88d7c7f4` and ran the guarded
  transaction again.
- Staged RBF SHA-256 matched the committed audit and remote copy. Armed and
  verified HPS watchdog PID 12320 with a 35-second menu rollback deadline.
- Loaded the custom RBF at 16:45:46 UTC. Live identity became
  `core=Zhaozhou Board Bring-up` / `rbf=Zhaozhou Board Bring-up`; FPGA manager
  remained `operating`; all three bridges remained enabled; SSH remained live.
- **Owner visually confirmed the color bars on the physical display.** This is
  whole-system likeness evidence for the intended first probe, not just a
  component check.
- The host rolled back at 16:45:51 UTC because its checker incorrectly expected
  `/tmp/RBFNAME` to contain the staged filename. MiSTer uses the declared core
  name there. MENU returned, all bridges/FPGA/SSH remained healthy, the HPS
  watchdog was disarmed, and the staged RBF was removed.
- Preserved the exact attempt as
  `FIRST-VOLATILE-LOAD-ATTEMPT2-BARS-ROLLBACK.json`. Its `status=failed` refers
  only to the wrong identity assertion; the load, visual output, and rollback
  all succeeded and are recorded independently.
- Corrected the RBF identity assertion to the observed MiSTer contract. No RTL
  or bitstream rebuild is needed.

### 2026-09-13 18:49 UTC+02:00 - Full guarded hardware transaction passed

- Replayed the exact audited RBF for the intended 20-second observation window.
- Preflight re-verified source/audit/RBF/menu hashes and found the remote staging
  path absent. Remote RBF bytes matched local SHA-256.
- Armed and verified HPS watchdog PID 12686 with a 35-second independent menu
  deadline before loading.
- At 16:48:32 UTC, live identity became `Zhaozhou Board Bring-up`; FPGA manager
  stayed `operating`; all bridges stayed enabled; SSH/network continuity held.
- After the full hold, host rollback returned live identity to MENU at
  16:48:57 UTC. Watchdog was disarmed before its deadline and emitted no fire
  log. Temporary remote RBF was removed and synced.
- Final `FIRST-VOLATILE-LOAD.json`: `status=ok`, `rollbackSucceeded=true`,
  `stagedFileRemoved=true`, `error=null`.
- **Milestone:** build -> stage -> volatile FPGA configure -> run Zhaozhou RTL ->
  observe -> HPS-verified rollback is proven on the physical SuperStation One.
  JTAG, configuration flash, boot persistence, and unsigned external I/O timing
  remain outside this proof.

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
- `tools/board/invoke_superstation_probe.ps1`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/QUARTUS-BUILD-AUDIT.json`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/QUARTUS-FLOW.rpt`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/QUARTUS-PIN.pin`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/QUARTUS-STA.rpt`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/ROLLBACK-REHEARSAL.json`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/FIRST-VOLATILE-LOAD-ATTEMPT1-NOLOAD.json`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/FIRST-VOLATILE-LOAD-ATTEMPT2-BARS-ROLLBACK.json`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/FIRST-VOLATILE-LOAD.json`

---

## Decisions Made

- Treat the connected SuperStation hardware as the current specification-test
  target, distinct from the later reference/debug FPGA board.
- Read-only USB/JTAG/network discovery is permitted now.
- The owner first asked to watch the first test, then explicitly said not to
  stop because it can be replayed. Proceed after the audit; preserve a complete
  receipt and keep the probe transaction repeatable.

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
