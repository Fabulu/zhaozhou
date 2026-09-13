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

### 2026-09-13 18:58 UTC+02:00 - Capability boundary and watchdog positive control

- Committed/pushed the full physical proof as `2955a9e1`.
- Wrote `reports/SUPERSTATION-ONE-CAPABILITY-MATRIX-20260913.md`: video,
  volatile loading, bridge/network continuity, and host rollback are proven;
  SDRAM, HPS-DDR-from-fabric, audio, controllers, GPIO/SNAC, JTAG, flash,
  persistence, and external I/O timing remain explicitly open.
- Did not mislabel the independent HPS watchdog as proven by silence. It was
  armed but had never reached its deadline, so its actual FIFO action was still
  untested.
- Added a separately named `-ExerciseWatchdog` transaction mode. It loads the
  already-audited RBF, observes the probe, deliberately withholds host rollback,
  requires the HPS process to return MENU and leave a `watchdog-fired` log, then
  cleans up. Host rollback remains the failure fallback.
- No new Quartus compile is planned; this uses the exact RBF already proved.

### 2026-09-13 19:04 UTC+02:00 - Watchdog fired but its first FIFO write blocked

- Ran the separately named watchdog positive control with the exact audited RBF.
  Owner again saw bars.
- HPS log proves the independent process reached its deadline at
  `17:00:50Z`, but the board still reported the probe after the observation
  window. The single shell write to `/dev/MiSTer_cmd` had blocked while MiSTer
  was recreating/reattaching the FIFO around the new core.
- Host fallback then returned MENU at `17:01:05Z`; FPGA manager, bridges, and
  SSH remained healthy; staged RBF and watchdog token/log were cleaned.
- Preserved `WATCHDOG-FIRE-TEST-ATTEMPT1-FIFO-BLOCKED.json`. It is a failed
  instrument test, not a failed FPGA probe.
- Replaced the one-shot writer with a staged HPS `/tmp` script: each attempt has
  a 3-second timeout, retries up to five times, waits for and checks MENU after
  a successful FIFO write, and logs attempts/write/result. Host rollback still
  owns the failure path. The script is base64-staged to avoid shell quoting and
  is removed after the transaction.

### 2026-09-13 19:07 UTC+02:00 - HPS watchdog positive control passed

- Committed/pushed the bounded retry repair as `90897195` and reran the exact
  audited RBF with a 10-second HPS watchdog deadline.
- Owner again saw bars; no new engine blocks were claimed because this was the
  same immutable RBF and a separately named rollback-instrument test.
- Watchdog fired at `17:06:20Z`, wrote the FIFO on attempt 1, waited for the
  MiSTer transition, and logged `watchdog-menu-ok=1`.
- HPS-reported identity was MENU at `17:06:30Z` before the host fallback path
  could act. FPGA manager, all bridges, and SSH remained healthy.
- Host cleanup disarmed/removed the ended watchdog artifacts and removed the
  staged RBF. `WATCHDOG-FIRE-TEST.json` is `status=ok`,
  `rollbackSucceeded=true`, `stagedFileRemoved=true`, `error=null`.
- The watchdog instrument has now been made to fire and catch the state it is
  supposed to catch; future volatile loads may rely on it with host fallback.

### 2026-09-13 19:34 UTC+02:00 - Real Zhaozhou hardware spec image authored

- Kept the proven `ZhaozhouBringup` project/RBF unchanged and created a separately
  named `ZhaozhouSpecs` project and load profile.
- Added `zhao_ssone_spec_tests`, which executes 16 fabric vectors against three
  existing shipping blocks: `zhao_crc32c_fold`, the formally proved
  `zhao_raster_fill`, and `zhao_dual18_mul` with the actual Cyclone V packed-DSP
  backend selected for synthesis.
- The runner latches the first failing vector and an independently accumulated
  `e5f1c57f` datapath signature. It preserves the three block instances; final
  Quartus audit must still prove their hierarchy and `cyclonev_mac` survived and
  that total DSP use increased from the framework's 33 to at least 34.
- Added a separately named MiSTer core/display: green signature bands mean all
  vectors and signature passed; red plus eight failure-code bands means failure.
  SDRAM/HPS-DDR requests/audio/user port remain inactive as in the proven probe.
- Added an isolated Verilator runner using the calibrated Windows MAKE/SHELL/C++
  environment. It initially emitted PASS and then a later `%Fatal` because
  `$finish` inside a repeat loop fell through; the runner also trusted exit 0.
  Rewrote the bench to one terminal check and made `%Fatal` an explicit failure.
- Clean result: 16 vectors pass, deliberate detector mutant latches fail code 1,
  signature is `e5f1c57f`, no fatal output.
- Added three spec-build verifier positive controls: missing shipping hierarchy,
  missing 34th packed DSP, and a valid cp1252 report fixture. All fire/pass as
  intended. Source/device/pin safety verifier passes.
- Added `build_superstation_specs.ps1`: exact branch/clean-source/no-competing-fit
  guards, isolated build directory, directed preflight, and post-map hierarchy/
  DSP plus final pin/timing/RBF audit. Its dirty-source guard fired deliberately.
- Generalized the guarded loader with a closed `Bringup|Specs` profile table;
  arbitrary RBF/project/audit combinations remain refused.

### 2026-09-13 19:36 UTC+02:00 - Real-block boundary compile started

- Committed and pushed the exact source as `c68cc8ad`.
- Started one isolated `ZhaozhouSpecs` compile after coordinating with the
  Packet-B/shell-fit lane. Expected duration is the probe fit's ~5 minutes; it
  is hours ahead of and may not overlap the pinned 23:00 shell fit.
- Build closure was copied to ignored checkout-local
  `build-board-superstation-specs/`; no shared build or Packet-B source is live.
- **Work in progress while fit runs:** prepare the separate build-audit and load
  receipt boundary from the existing profile; do not edit the copied source
  closure or start another Quartus process.
- **Next after result:** record this parked state, inspect hierarchy/DSP/pins/
  timing, preserve exact key reports, commit the audit, then use only the Specs
  load profile with the proven watchdog transaction.
- Wrote `SUPERSTATION-HARDWARE-SPECS-V1-20260913.md` while the copied fit ran.
  It limits any green result to these 16 selected vectors plus post-map primitive
  presence. Mixed-sign expansion, 32x18 recombination, CE/reset sequencing,
  random corpus, raw host-observed lanes, physical mutants, production migration,
  and DSP saving remain separate claims. The shell-fit lane accepted this exact
  boundary.

### 2026-09-13 19:42 UTC+02:00 - Specs fit result arrived; work parked first

- Background `ZhaozhouSpecs` build reports success.
- **Work completed while fit ran:** existing committed
  `tests/dsp/run_dual18_verilator.py --quick` passed all backend/sign-mode/
  recombination/CE/reset/lane-swap controls and emitted its distinct transcripts.
  It still correctly reports `DUAL18_VENDOR_GATE HOLD`; none of that simulator
  evidence is promoted into this physical v1 claim.
- **Parked state:** no source changed after fit snapshot `c68cc8ad`; exact v1
  claim boundary is committed as `f1326088`; load profile already expects a
  separately committed `HARDWARE-SPECS-BUILD-AUDIT.json`.
- **Resume now:** inspect the completed map hierarchy, physical MAC/DSP count,
  pins, timing, and RBF hash; preserve raw key reports and audit before loading.

### 2026-09-13 19:48 UTC+02:00 - Real-block RBF audit passed

- Quartus full flow passed in 4m35s from exact source
  `c68cc8ad3e480af6a8a0a5f3dcaffc81b7165d20`: zero errors, zero critical
  warnings, `5CSEBA6U23I7`, `sys_top`, 7,265 ALMs, 11,157 registers,
  384,498 block-memory bits, 34 DSPs, 145 physical/0 virtual pins, 3 PLLs.
- Strengthened the post-map gate after the fit: exact resource-table rows prove
  `zhao_crc32c_fold:u_crc` survived as 133 ALUTs,
  `zhao_raster_fill:u_fill` as 1 ALUT, and `zhao_dual18_mul:u_mul` as exactly
  1 DSP under the 317-ALUT/44-register test runner. The direct instance names
  `cyclonev_mac:u_dual18_mac`.
- Did not cite `preserve_hierarchy` as evidence: Quartus 17 warns that attribute
  is unrecognised. Actual hierarchy/resource rows and implemented DSP count are
  the evidence.
- Pin audit: canonical 3.3-V clock inputs V11/Y13/E11; 169 reserved inputs;
  zero reserved outputs; no missing location/I/O-standard warnings.
- Timing positive: setup +0.648 ns, hold +0.250 ns, recovery +3.243 ns,
  removal +0.811 ns, min pulse +1.122 ns; zero illegal/unconstrained clocks.
  External 4 input/14 path and 50 output/122 path board delays remain unsigned.
- RBF: 2,425,124 bytes, SHA-256
  `59407e97e208980c7965b931bcaf920291cadb40f56c623ec32cc3df672c86cd`.
- Preserved exact flow/map/pin/STA reports and all flow/map/fit/asm/STA/pin/RBF/
  SOF hashes in `HARDWARE-SPECS-BUILD-AUDIT.json`. No second Quartus job.
- Physical load remains pending until this audit and strengthened checker are
  committed/pushed; then only the closed Specs profile may stage this hash.

### 2026-09-13 19:52 UTC+02:00 - Real-block image ran; visual result pending

- Committed/pushed exact build evidence as `923a1bba`, then invoked only the
  closed `Specs` load profile with RBF SHA-256
  `59407e97e208980c7965b931bcaf920291cadb40f56c623ec32cc3df672c86cd`.
- Staged bytes matched; HPS watchdog PID 18405 armed for 45 seconds.
- At `17:51:11Z`, MiSTer reported `Zhaozhou Hardware Specs`; FPGA manager stayed
  `operating`; all three bridges and SSH stayed live.
- Held 30 seconds, then host rollback returned MENU at `17:51:46Z`; watchdog was
  disarmed without firing; staged RBF was removed. Machine receipt is `status=ok`.
- Machine receipt proves load/identity/continuity/rollback, not HDMI pixels.
  Owner reported **green signature bands** at 19:53 local. Under the committed
  display contract, this means all 16 selected physical comparisons left first
  failure zero and the accumulated actual-result signature matched `e5f1c57f`.
- This closes only v1 selected CRC/fill/signed-A+unsigned-B packed-multiply
  vectors in this wrapper. Expanded physical arithmetic, raw host-readable
  lanes, physical mutants, production migration/saving, and the full shell
  remain open.

### 2026-09-13 20:00 UTC+02:00 - Machine-readable partial board truth

- Created the contract-named `reports/board_truth.json` rather than leaving the
  day's evidence only in prose/run folders.
- Encoded confirmed exterior identity, power label, FPGA compatibility target,
  absent JTAG chain, HPS/FPGA clocks, reset/load method, HPS memory map,
  network/software identity, physical probe/watchdog/spec results, and exact
  evidence paths/commit.
- Kept PCB/FPGA markings, negotiated PD/rails, oscillator parts, physical reset
  mapping, FPGA SDRAM part/timing/bandwidth, HPS fabric-DDR, audio/input/GPIO,
  analog video, external timing, JTAG/flash/persistence, and full shell in an
  explicit `openCapabilities` set. Overall status remains `partial`.
- Did not emit `sdram_params.svh`; no unmeasured board number has crossed the
  `ZH-004` seam into architecture constants.
- Added validator/cross-receipt checks and four fired refusal controls for
  premature SDRAM closure, flash-access claims, disappearing open capabilities,
  and current truth. All pass.

---

## Subagent Spawns

None. This task is restricted to the dedicated Claude Code hardware session.

---

## Files Created

- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/TASK_LOG.md`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/SPEC_v1.md`
- `reports/BOARD-BRINGUP-SUPERSTATION-ONE-20260913.md`
- `reports/SUPERSTATION-ONE-CAPABILITY-MATRIX-20260913.md`
- `reports/SUPERSTATION-HARDWARE-SPECS-V1-20260913.md`
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
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/WATCHDOG-FIRE-TEST-ATTEMPT1-FIFO-BLOCKED.json`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/WATCHDOG-FIRE-TEST.json`
- `fpga/ZhaozhouSpecs.qpf`
- `fpga/ZhaozhouSpecs.qsf`
- `fpga/ZhaozhouSpecs.sdc`
- `fpga/files_specs.qip`
- `fpga/rtl/platform/zhao_ssone_spec_tests.sv`
- `fpga/rtl/platform/zhao_ssone_specs_emu.sv`
- `tests/board/ssone_spec_tests_tb.sv`
- `tests/board/run_ssone_spec_tests.py`
- `tests/tools/test_verify_superstation_specs.py`
- `tools/board/verify_superstation_specs.py`
- `tools/board/build_superstation_specs.ps1`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/HARDWARE-SPECS-BUILD-AUDIT.json`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/SPECS-QUARTUS-FLOW.rpt`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/SPECS-QUARTUS-MAP.rpt`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/SPECS-QUARTUS-PIN.pin`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/SPECS-QUARTUS-STA.rpt`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/HARDWARE-SPECS-LOAD.json`
- `reports/board_truth.json`
- `tools/board/validate_board_truth.py`
- `tests/tools/test_validate_board_truth.py`

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

1. Keep the v1 proof immutable. A separately named expanded hardware-spec gate
   must add host-readable raw lanes, mixed-sign/32x18 vectors, bounded random
   corpus, and physical lane-swap/collapse/ownership controls before broad
   packed-arithmetic closure.
2. Bring up board interfaces one question at a time, starting with the 128 MB
   FPGA SDRAM component/geometry/timing/read-write test, then controller input,
   HPS fabric-DDR, audio, and Z60/Storm/Duo video timing. Do not emit measured
   `sdram_params.svh` until real measurements exist.
3. Replace the shell's 3,214-virtual-pin characterization boundary with real
   MiSTer HPS command, memory, and framebuffer interfaces; integrate Packet B
   only after its separate lane lands.
4. Run reference-oracle workloads through command -> geometry/field/raster ->
   framebuffer and preserve physical counters/captures.
5. Keep JTAG, configuration flash, persistent boot, and external-I/O timing
   blocked behind separately named evidence. If the PCB later becomes safely
   accessible for another reason, capture its silk revision and FPGA top mark.
6. Do not start another Quartus job before the separate lane's pinned 23:00
   shell-fit window is over.
