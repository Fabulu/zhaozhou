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

### 2026-09-13 20:59 UTC+02:00 - Independent review HOLD and repair packet

- Owner relayed the independent review of pushed snapshot `fe684755`. Existing
  results remain credible: source/RBF/load hashes agree, physical green implies
  all 16 fixed checks plus `e5f1c57f`, one packed dual18 DSP is mapped, rollback
  succeeded, and no JTAG/flash/persistence occurred.
- **HOLD:** no Quartus compile, RBF load, JTAG, programming, or board/SD change
  until the repair packet is reviewed and tonight's 23:00 shell fit has passed.
- Blocker 1: pinned upstream `sys_top.v:1661-1667` conditionally drives USER/SNAC
  low, including SW[1]-selected audio signals on bits 2/4/5. Repair must preserve
  the vendored tree and apply a deterministic board-build copy patch making all
  seven pins unconditionally high-impedance.
- Blocker 2: `ascal.mode` is 5 bits while `sys_top.v:791` supplies 4. Quartus
  emitted an unnumbered/tabular Critical Warning. Add the missing leading zero
  without changing existing mode-bit semantics; make every Critical Warning
  spelling fatal and fire it with a committed fixture mutant.
- Blocker 3: loader trusts ambient `known_hosts` and loosely interpreted state.
  Pin the documented ED25519 fingerprint plus unit identity; require exact core,
  FPGA `operating`, all three named enabled bridges, one live MiSTer PID, and
  complete watchdog fired/write/MENU evidence where watchdog mode is claimed.
- Add a complete source/build-copy/report/RBF/SOF hash manifest and make the
  loader consume it. Correct/invalidate earlier false-green build audit status
  without erasing the credible historical physical observations.
- After repair/review and the 23:00 fit, next physical gate is red/failure control
  followed by green replay; then a nonce-selected HPS raw mailbox with independent
  host-readable lane results. Repair-only work begins now in this checkout.

### 2026-09-13 21:48 UTC+02:00 - Repair-only packet complete, compile/load still held

- Preserved the 57-file upstream MiSTer tree exactly. Added a deterministic
  build-copy patch with pinned input/output SHA-256. It adds the scaler's fifth
  leading-zero bit and makes all seven USER/SNAC pins unconditional high-Z.
- Both board build scripts apply the overlay after copying `sys/`; no vendored
  source was edited. Future verifier requires the exact patched digest and all
  seven permanently disabled USER_IO output enables.
- Replaced numbered-only Critical Warning parsing with all-format scanning across
  every report. Added tabular-warning positive controls to both project verifiers.
- Re-audited the historical Specs workspace: repaired verifier correctly rejects
  its unpatched sys_top, SW[1] drive path, 4-bit scaler mode, tabular Critical
  Warning, and incomplete `[0,1,3,6]` USER_IO output-disable set.
- Marked both historical build audits `historical-invalidated` with future load
  authorisation false while retaining historical physical-result credibility.
- Pinned loader ED25519 fingerprint and isolated it from ambient known-hosts;
  pinned hostname, DT model/compatibility, MAC, HPS silicon revision, MiSTer and
  MENU hashes. Exact before/load/rollback state now requires core+RBF identity,
  FPGA operating, three named enabled bridges, UTC, and one numeric MiSTer PID.
- Physical loads now require watchdog-primary mode, a never-before-used explicit
  receipt path, V2 audit, complete V2 manifest, and exact watchdog
  fired/attempt/write/MENU evidence.
- First read-only identity attempt exposed PowerShell 5 native stderr handling;
  explicit Process capture repaired it. Second pinned identity preflight passed.
- A read-only positive control aimed the repaired loader at the historical Specs
  RBF. It rejected before manifest/audit, SCP, watchdog arm, or `load_core` due
  source/build mismatch. Receipt records `loaded=[]` and no remote path.
- Added complete manifest tooling to both build scripts: source commit, every
  source/verification/build-input hash, exact patched sys_top, every Quartus
  output file, required reports/RBF/SOF, and canonical self-digest. Source,
  artifact, and self-digest mutants fire.
- Corrected `board_truth.json`, capability matrix, provenance, inventory, v1
  spec report, and historical audit records to carry the review HOLD and not
  erase credible observations.
- 36 repair/unit/mutation tests pass; both source verifiers and held board-truth
  validator pass; PowerShell/Python syntax checks pass. No Quartus process,
  compilation, RBF staging/load, JTAG, flash, or board/SD mutation occurred.
- Committed and pushed the source/tool/test repair as `9f75c931`; this is a
  review candidate only, not compile or load authorisation.

### 2026-09-13 23:51 UTC+02:00 - Completeness/provenance review gaps closed

- Independent audit of `a6e7b488` kept the HOLD and identified four remaining
  evidence gaps. Gap 1—no repaired V2 build/audit/manifest—is intentionally open
  until source review and the separate 23:00 fit finish; no compile was run.
- Manifest verifier now reconstructs and requires exact profile source,
  verification, build-input and exact 16-file Quartus output name sets. It
  validates every size/hash, canonical self-digest, and independently rebuilt
  `sourceManifestSha256`.
- Added fired mutants that remove a source or add an output and then recompute
  the manifest self-digest; both now fail, as do source-manifest digest, raw
  source, artifact, and self-digest mutations.
- Receipt verifier now refuses false `rollbackAttempted`, empty raw loaded/
  rollback arrays, null remote RBF/build source, mismatched audit/manifest/source
  bindings, old manifest names, raw-vs-parsed state drift, disabled bridges, and
  noncontiguous/incomplete watchdog attempts.
- Loader now refuses to produce identity or load evidence unless its own file is
  clean at HEAD, and records source commit, exact Git blob, and working SHA-256.
  Physical receipts require exact V2 audit/manifest names and bindings.
- Committed/pushed these source fixes as `bd734285`. Preserved the prior
  `fe684755` identity receipt as pre-source-binding evidence, then ran a new
  **read-only** preflight bound to loader commit `bd734285`, blob
  `98fbb8f856e18cbb5faab01fee085399062fff00`, and working SHA-256
  `692e735d0f8868723fa9c40a09770d1347cadf40b267ce8402354f37ce3b2d9b`.
- New source-bound receipt pins the expected ED25519/unit identity and exact MENU
  FPGA/bridge/PID state; independent receipt validator resolves and matches the
  commit:path blob. No RBF, remote path, watchdog, rollback, or board mutation.
- 45 repair/unit/mutation tests pass. Repair source/truth/identity gates pass.
  No Quartus compile, staging/load, JTAG, flash, persistence, or SD mutation.
- Pushed manifest/receipt completeness source as `bd734285` and the
  board-truth-to-identity binding verifier as `bb441091`. Both remain
  repair-review candidates, not compile/load authorisation.

### 2026-09-14 03:56 UTC+02:00 - Actual V2 evidence verifier repair pushed

- Latest independent review kept the physical/Quartus HOLD and found two
  provenance holes: the build scripts dot-sourced `tools/env/zhao-env.ps1`
  outside their manifest closure, while the future-receipt verifier accepted
  format-correct but nonexistent V2 evidence/build commits and arbitrary summary
  digests.
- Both build-script dirty closures and both production manifest source profiles
  now bind `tools/env/zhao-env.ps1`.
- Future-load receipt verification now resolves the exact profile-specific V2
  audit and complete manifest inside the repository; requires both to be clean,
  committed at the receipt source commit, and byte-identical to those blobs;
  resolves the build source commit; invokes the complete manifest verifier; and
  recomputes the local RBF size/SHA-256 before cross-checking receipt, manifest,
  and audit identities.
- Complete-manifest verification additionally binds source/verification content
  to `sourceCommit` and directly checks the build-copy `sys_top.v` record against
  both the actual file and build-input record.
- Added fired controls for nonexistent evidence/commit, arbitrary manifest
  digest, local-RBF mutation, uncommitted audit mutation, source-commit drift,
  and detached patched-sys_top provenance.
- 58 repair/unit/mutation tests pass. Both source verifiers, board-truth gate,
  source-bound identity receipt, Python syntax, and PowerShell syntax pass.
- Source/tool/test repair commit
  `6d0a2a8846f2eb250dc828b02d9931d3bfbbb060` is pushed on the dedicated branch.
- No Quartus process was running. No compile, RBF staging/load, SSH mutation,
  JTAG, flash, persistence, board pin drive, or board/SD mutation occurred.
  Repaired V2 build/audit/manifest remain intentionally absent pending review.

### 2026-09-14 04:22 UTC+02:00 - Loader source/working byte false-pass closed

- Independent audit rejected `57dc5ab0`: repository-backed identity validation
  independently accepted the current clean working SHA and a named historical
  commit/blob, but did not require those two byte identities to match.
- Repaired validation now reads the loader blob bytes from the receipt's exact
  `sourceCommit`, hashes them, and requires both direct equality with the clean
  working loader and equality with `loaderSource.workingSha256`.
- Quarantined format-only identity checking under
  `validate_identity_structure`; production `validate_identity_receipt` now
  fails closed when no repository is supplied.
- Added a hostile control combining real pre-binding commit
  `fe684755b8076b005214dc8dbbad7195b678b9f0`, its real differing loader blob
  `32e22e0f527ebdd2245a002b8951dfee84b94262`, and the current loader working
  SHA. Both the committed/working SHA check and byte comparison fire. A separate
  control proves repository-free production validation fails.
- Receipt verifier tests pass 23/23; the complete focused repair suite passes
  60/60. Source, truth, source-bound identity, and syntax gates pass.
- Repair commit `e66a606636727fb4df27aed87bd1181af01164eb` is pushed.
  No Quartus process, compile, RBF/load, SSH mutation, JTAG, flash, persistence,
  pin drive, or board/SD mutation occurred. HOLD remains.

### 2026-09-14 04:53 UTC+02:00 - One repaired V2 Specs compile authorised and started

- Independent re-review accepted exact pushed head
  `0ba2eefcd2938cd6a50a1853380ab4d8c9b8a629` for exactly one compile-only
  action. Physical activity remains HOLD.
- A manual head guard initially refused before invoking the build because I
  mistyped the accepted full hash in the comparison. I corrected only that
  command literal; the checkout remained clean and no compile had started.
- Confirmed local/remote accepted head equality, clean tree, absent dedicated
  build path, and no running Quartus process.
- Started the committed manifest-bound `build_superstation_specs.ps1` once at
  04:53:46 in new ignored build directory
  `build-board-superstation-specs-v2-0ba2eefc`; `quartus_sh` launched
  `quartus_map`.
- **Checkpoint before reading the fit:** when the command completes, first
  preserve its raw output and build workspace, then require repaired Specs
  verification, exact complete-manifest verification, zero Critical Warnings,
  source/profile/RBF closure, and positive timing. Create/push V2 audit,
  manifest, compile receipt, and raw-report copies only if every gate passes.
  Do not stage/load the RBF or perform any SSH/JTAG/flash/SD action.

### 2026-09-14 06:27 UTC+02:00 - Compile measured; complete-manifest gate failed closed

- The single build entry ended RC=1. Quartus itself completed successfully at
  04:58:14: target `5CSEBA6U23I7`, 0 errors, 55 warnings, 0 Critical Warnings.
  The repaired post-build verifier passed.
- Resources: 7,312 ALMs, 11,173 registers, 384,498 block-memory bits, 34 DSPs,
  145 pins, 0 virtual pins and 3 PLLs. All USER_IO output enables 0–6 were
  disabled and reserved outputs were zero. Zhaozhou hierarchy included one DSP
  in `zhao_dual18_mul:u_mul`.
- Internal slacks were setup +0.058 ns, hold +0.253 ns, recovery +3.658 ns,
  removal +0.858 ns and minimum pulse width +1.122 ns. Illegal/unconstrained
  clocks were zero; external I/O timing remains unsigned.
- Measured RBF: 2,448,816 bytes, SHA-256
  `31699ff37440f26c8a979f53ce45b02ac63185038a2cc51a139eaba9ecb491eb`.
- Overall evidence correctly failed: `quartus_sh --flow` rewrote the copied QSF
  after source capture. It changed 3,184-byte SHA-256
  `0a4f036ec7aab3faf7a8b74a823add7485714107913b9156db690a1e9feebdd5` to
  3,323-byte SHA-256
  `c772638c5ac0ddf73739b1f4f47dbcaeb73a4c8218fce6a367556e2f1123841b`,
  changing the edition string and appending
  `RESERVE_ALL_UNUSED_PINS_NO_OUTPUT_GND`. Complete-manifest creation refused
  the `ZhaozhouSpecs.qsf` build-input hash/size mismatch. No complete candidate
  exists; the RBF is quarantined.
- Preserved source manifest, complete transcript, repaired verifier JSON,
  `failed:manifest` audit/compile receipt, all 11 raw Quartus reports/summaries/
  message files, and both QSF states. The programming artifacts remain only in
  the ignored quarantined build workspace; the audit records all 16 output
  hashes. No rerun occurred.
- Repaired both build entries to use `run_superstation_quartus.py`: map/fit/asm
  receive `--read_settings_files=on --write_settings_files=off`; every stage is
  guarded by exact pre/post QSF byte equality. Added controls for exact compiler
  arguments, entry-point use, clean stage sequence and the observed QSF mutation.
  Helper/tests are bound into both manifest profiles. The complete focused suite
  passes 64/64; source, truth, bound-identity and PowerShell/Python syntax gates
  pass. Repair commit `44a6d88576be382bf5eaa69ea0cf10fcc6d1982d`
  is pushed, has not been compiled, and awaits independent source review.
- No RBF staging/load, SSH mutation, watchdog/rollback, JTAG, flash,
  persistence, pin drive or board/SD mutation occurred. Physical HOLD remains.

### 2026-09-14 06:46 UTC+02:00 - One no-write V2 Specs retry authorised and started

- Independent Luna review accepted exact clean pushed head
  `8c91b784ed1f2a3079b7d7802273203be336af5c` and authorised exactly one
  compile-only retry through the committed stage helper.
- Confirmed clean local/remote head equality, a new absent attempt-2 build path,
  and no existing Quartus process. Started `build_superstation_specs.ps1` once
  with build directory `build-board-superstation-specs-v2-attempt2-8c91b784`.
- **Checkpoint before result:** preserve the transcript and workspace before
  inspection. Require stage RCs zero, exact QSF byte identity across all stages,
  source/complete manifest closure, zero Critical Warnings, repaired post-build
  verification and exact RBF/audit/receipt bindings. If any gate fails, preserve
  and stop without repeat or bypass.
- Compile-only boundary remains absolute: no RBF staging/load, SSH mutation,
  JTAG, flash, persistence, pin drive, watchdog/rollback or board/SD mutation.

### 2026-09-14 06:46 UTC+02:00 - Attempt 2 failed closed before map

- Preflight passed: source verifier, 16 vectors, mutant code 1, signature
  `e5f1c57f`, exact patched sys_top, and source-manifest capture with canonical
  digest `9ff77ce42eef6a633af3e048863a8f0011d947a363442f52e40a056bc1f9b017`.
- The helper's first build-ID command returned RC 0, then exact QSF identity
  failed. The pinned upstream `build_id.tcl` opens/closes the project; Quartus
  changed QSF SHA-256 from
  `0a4f036ec7aab3faf7a8b74a823add7485714107913b9156db690a1e9feebdd5` to
  `c772638c5ac0ddf73739b1f4f47dbcaeb73a4c8218fce6a367556e2f1123841b`.
- Stage RCs: build-ID 0, QSF guard 1, build entry 1; map/fit/asm/STA not run.
  Therefore attempt 2 has no Quartus measurement, raw reports, resources,
  timing, SOF, RBF, post-build verifier result or complete manifest.
- Preserved exact source manifest/transcript/QSF states plus generated
  `build_id.v`, unused `jtag.cdf` and database-info file under immutable
  `ATTEMPT2-BUILD-ID-QSF-MUTATION` names. No repeat or bypass occurred.
- No RBF staging/load, SSH mutation, JTAG use, flash, persistence, pin drive,
  watchdog/rollback or board/SD mutation occurred. Further compile and all
  physical activity remain HOLD pending independent review.

### 2026-09-14 07:12 UTC+02:00 - Projectless build-ID repair pushed

- Preserved the pinned 57-file vendor tree. Added a deterministic build-copy-only
  patch for `build_id.tcl`: pinned input SHA-256
  `148dc6a8124d36ac2898a45d085960cc05178a76d7297fde8877925fc0a74e88`,
  projectless output SHA-256
  `e9a3daa3d507075abf214fefa115505a7669a14898800b728492f8a4393272c6`.
- The copied script contains no `project_open`, `project_close` or
  `get_global_assignment`; revision, device `5CSEBA6U23I7` and `output_files`
  are passed as explicit source-bound arguments.
- Both build entries patch it before source-manifest capture. Stage runner,
  source verifier, build verifier and complete-manifest verifier require the
  exact projectless digest and copied build-input identity.
- Added controls for exact input/output and mutation/missing/double-patch
  refusal, project-access absence, exact production arguments, immediate
  observed-mutation rejection, detached manifest record, and an actual Quartus
  17 projectless build-ID invocation that leaves QSF bytes unchanged.
- Full focused suite passes 71/71; both source verifiers, board truth,
  source-bound identity, Python syntax and PowerShell syntax pass.
- Repair commit `2ee7d560d216170a791286b35a871573f8a8581a` is pushed. No compiler
  stage or physical/SSH/JTAG/flash/SD action occurred. Compile and physical HOLD
  remain pending independent review.

### 2026-09-14 10:59 UTC+02:00 - Projectless V2 Specs attempt 3 started

- Independent review accepted exact clean pushed head
  `5666bce4985b9cfa9c5e0139b540364893b08450` and authorised exactly one
  compile-only attempt through the projectless build-ID/stage-isolated runner.
- Confirmed clean local/remote equality, absent new attempt-3 build path and no
  existing Quartus process. Started the committed build entry once in
  `build-board-superstation-specs-v2-attempt3-5666bce4`.
- Projectless build-ID passed its QSF guard; `quartus_map` began at 10:59:33.
- **Checkpoint before result:** preserve transcript/workspace first. Require every
  stage RC and QSF guard, repaired post-build verifier, complete manifest,
  source/RBF identity and all artifact hashes. Any failure is immutable and
  terminal for this authorization; no repeat/bypass.
- Compile-only boundary: no RBF staging/load, SSH mutation, JTAG, flash,
  persistence, pin drive, watchdog/rollback or board/SD mutation.

### 2026-09-14 11:12 UTC+02:00 - Attempt 3 failed exact artifact-set closure

- Projectless build-ID, map, fit, assembly, STA, stage helper and repaired
  post-build verifier all returned RC 0. Every QSF guard passed; source/final
  remained 3,184 bytes with SHA-256
  `0a4f036ec7aab3faf7a8b74a823add7485714107913b9156db690a1e9feebdd5`.
- Quartus measurements: 0 errors, 55 warnings, 0 Critical Warnings; 7,312 ALMs,
  11,173 registers, 384,498 memory bits, 34 DSPs, 145 pins, 0 virtual pins,
  3 PLLs; all USER_IO output enables 0–6 disabled and expected Zhaozhou
  hierarchy/packed DSP present.
- Timing: setup +0.058 ns, hold +0.253 ns, recovery +3.658 ns, removal
  +0.858 ns, minimum pulse width +1.122 ns; illegal/unconstrained clocks zero.
- RBF: 2,448,816 bytes, SHA-256
  `31699ff37440f26c8a979f53ce45b02ac63185038a2cc51a139eaba9ecb491eb`.
- Overall entry returned RC 1 at complete-manifest creation. The direct stage
  sequence emitted 15/16 exact outputs; required `ZhaozhouSpecs.done`, normally
  created by `quartus_sh --flow`, was absent. The exact record-set gate refused;
  no complete manifest exists and the RBF is quarantined.
- Preserved immutable `ATTEMPT3-MISSING-DONE` source manifest, failed audit,
  receipt, transcript, verifier result, 11 textual raw reports, both QSF states
  and build-ID files. No repeat/bypass occurred.
- No RBF staging/load, SSH mutation, JTAG, flash, persistence, pin drive,
  watchdog/rollback or board/SD mutation occurred. Further compile and physical
  activity remain HOLD.

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
- `reports/BOARD-BRINGUP-REVIEW-REPAIR-20260913.md`
- `tools/board/patch_mister_sys_top.py`
- `tools/board/superstation_build_manifest.py`
- `tools/board/verify_superstation_receipt.py`
- `tests/tools/test_patch_mister_sys_top.py`
- `tests/tools/test_superstation_build_manifest.py`
- `tests/tools/test_verify_superstation_receipt.py`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/IDENTITY-PREFLIGHT-ATTEMPT1-KEYSCAN-STDERR.json`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/IDENTITY-PREFLIGHT-PRE-SOURCE-BINDING.json`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/IDENTITY-PREFLIGHT.json`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/LOAD-HOLD-OLD-RBF-REJECT.json`
- `tools/board/run_superstation_quartus.py`
- `tests/tools/test_run_superstation_quartus.py`
- `tools/board/patch_mister_build_id.py`
- `tests/tools/test_patch_mister_build_id.py`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/HARDWARE-SPECS-SOURCE-MANIFEST-V2-ATTEMPT1-QSF-MUTATION.json`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/HARDWARE-SPECS-BUILD-AUDIT-V2-ATTEMPT1-QSF-MUTATION.json`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/HARDWARE-SPECS-COMPILE-RECEIPT-V2-ATTEMPT1-QSF-MUTATION.json`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/SPECS-V2-ATTEMPT1-QSF-MUTATION-COMPILE-TRANSCRIPT.txt`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/SPECS-V2-ATTEMPT1-QSF-MUTATION-VERIFY.json`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/SPECS-V2-ATTEMPT1-QSF-MUTATION-QUARTUS-RAW/`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/SPECS-V2-ATTEMPT1-QSF-MUTATION-BUILD-REPORT.md`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/SPECS-V2-ATTEMPT1-QSF-MUTATION-EVIDENCE-INDEX.md`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/HARDWARE-SPECS-SOURCE-MANIFEST-V2-ATTEMPT2-BUILD-ID-QSF-MUTATION.json`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/HARDWARE-SPECS-BUILD-AUDIT-V2-ATTEMPT2-BUILD-ID-QSF-MUTATION.json`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/HARDWARE-SPECS-COMPILE-RECEIPT-V2-ATTEMPT2-BUILD-ID-QSF-MUTATION.json`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/SPECS-V2-ATTEMPT2-BUILD-ID-QSF-MUTATION-COMPILE-TRANSCRIPT.txt`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/SPECS-V2-ATTEMPT2-BUILD-ID-QSF-MUTATION-RAW/`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/SPECS-V2-ATTEMPT2-BUILD-ID-QSF-MUTATION-BUILD-REPORT.md`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/SPECS-V2-ATTEMPT2-BUILD-ID-QSF-MUTATION-EVIDENCE-INDEX.md`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/HARDWARE-SPECS-SOURCE-MANIFEST-V2-ATTEMPT3-MISSING-DONE.json`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/HARDWARE-SPECS-BUILD-AUDIT-V2-ATTEMPT3-MISSING-DONE.json`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/HARDWARE-SPECS-COMPILE-RECEIPT-V2-ATTEMPT3-MISSING-DONE.json`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/SPECS-V2-ATTEMPT3-MISSING-DONE-COMPILE-TRANSCRIPT.txt`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/SPECS-V2-ATTEMPT3-MISSING-DONE-VERIFY.json`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/SPECS-V2-ATTEMPT3-MISSING-DONE-RAW/`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/SPECS-V2-ATTEMPT3-MISSING-DONE-BUILD-REPORT.md`
- `runs/CLAUDE-RUNS/RUN-20260913-1651-board-bringup/SPECS-V2-ATTEMPT3-MISSING-DONE-EVIDENCE-INDEX.md`

---

## Decisions Made

- Treat the connected SuperStation hardware as the current specification-test
  target, distinct from the later reference/debug FPGA board.
- Read-only USB/JTAG/network discovery is permitted now.
- The owner first asked to watch the first test, then explicitly said not to
  stop because it can be replayed. Those historical loads completed; independent
  review now supersedes that standing state with a HOLD on all future physical
  loads until repaired V2 evidence is approved after the 23:00 fit.

---

## Next Steps

1. Commit/push immutable attempt-3 missing-`done` evidence and submit exact head
   for independent review.
2. Do not repair completion-marker generation or run Quartus again without
   reviewed successor source and separate compile authorization; keep the exact
   16-artifact set unchanged.
3. A later retry must preserve byte-identical compiled QSF, exact patched
   sys_top digest, zero Critical Warnings, all seven USER_IO output enables
   disabled, positive timing, physical hierarchy, and a complete V2
   source/build-input/report/RBF/SOF manifest.
4. Commit the successful V2 audit/manifest and obtain explicit independent
   approval before any physical action.
5. Physical red/failure control, green replay, and HPS nonce-selected raw mailbox
   remain later separately authorised gates. Keep broader arithmetic,
   migration/saving, SDRAM, JTAG, flash, persistence, external timing, and full
   shell claims open until their own gates.
