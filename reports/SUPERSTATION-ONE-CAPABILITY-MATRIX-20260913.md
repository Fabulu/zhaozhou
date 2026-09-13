# SuperStation One capability matrix — 2026-09-13

**Board:** Retro Remake SuperStation One, exterior model marking
`RCSH-1001/1002`
**Volatile probe RBF:** SHA-256
`7e7b46f79685383dc85a057f88602154039e27cf4bea37fdfb911a55948ea9c0`
**Physical proof commit:** `2955a9e1`
**Independent review:** **HOLD all future physical loads** until repaired V2
build/audit/manifest and loader gates pass after the 23:00 shell fit.

A green row means the named question was exercised on the physical unit. It does
not generalise to a neighbouring interface.

| Capability | State | Physical evidence | Boundary / next test |
|---|---|---|---|
| Product identity | **Confirmed** | Owner read Retro Remake Hong Kong Limited, SuperStation One, `RCSH-1001/1002`, 5/9 V DC label | PCB revision and FPGA top marking remain unread |
| LAN discovery | **Confirmed** | Router DNS `mister.fritz.box`, `192.168.178.59` | DHCP address can change |
| SSH/HPS reachability | **Historical load continuity credible; target pin repaired read-only** | SSH succeeded before/during/after prior loads; repaired identity preflight now pins ED25519 fingerprint, hostname, DT identity, MAC, silicon ID, MiSTer and MENU hashes | Future load receipts must carry the strict parsed identity/state objects |
| FPGA device compatibility | **Confirmed for MiSTer RBF** | Audited `5CSEBA6U23I7` RBF configured and ran | Does not reveal physical ordering suffix |
| MiSTer framework | **Confirmed** | Declared core identity appeared as `Zhaozhou Board Bring-up` | Framework source is pinned; future updates need a new receipt |
| Volatile HPS loading | **Historical proof credible; future loads held** | Prior staged hashes and `/dev/MiSTer_cmd` transitions agree | Requires repaired V2 audit, complete manifest, strict identity/state/watchdog receipt, and review approval |
| HDMI-visible video | **Confirmed by owner** | Owner reported visible Zhaozhou color bars | Display-reported output mode/photo and raw HDMI timing were not captured |
| Analog video | **Open** | No observation | Test each intended VGA/component/composite/S-Video mode separately |
| Core clock/reset | **Confirmed for probe** | Pinned PLL locked sufficiently to run raster/HPS protocol; reset/load/reload worked | Production GPU/video/audio frequencies and domain reset sequencing remain open |
| HPS↔FPGA bridges | **Confirmed across load** | `lwhps2fpga`, `hps2fpga`, `fpga2hps` stayed enabled | No production command/data transaction crossed them yet |
| Host-driven MENU rollback | **Confirmed twice** | MENU identity returned after both custom loads | Known `menu.rbf` hash is pinned in receipts |
| HPS rollback watchdog | **Historical positive control credible; future receipt strengthened** | Independent HPS log contains fired/write-ok/MENU-ok and MENU was observed | Future loads must parse exact contiguous attempts plus matching write/MENU attempt and exact post-state |
| FPGA-side 128 MB SDRAM | **Open / intentionally untouched** | DQ output enables were disabled; no memory transaction | Run a separately audited address/data test before using it |
| HPS DDR from fabric | **Open / intentionally inactive** | All DDRAM request outputs held zero | Linux HPS memory working is not fabric-interface proof |
| Audio | **Open / intentionally silent** | Core outputs held zero | Test digital and analog paths separately at bounded level |
| Controller/input | **Open** | No controller transaction consumed | Test MiSTer input mapping and then native Zhaozhou snapshot path |
| User port / SNAC / GPIO | **Historical image unsafe for attached peripheral** | Review found `sys_top` can drive bits 2/4/5 low from physical SW[1] despite core `USER_OUT='1` | Build-copy safety overlay now makes all seven bits unconditional high-Z; a new fit must prove all seven output enables disabled before any load |
| SD SPI from core | **Open** | Core requested high impedance; framework retains its standard mux behavior | No Zhaozhou SD transaction performed |
| LEDs | **Not owner-confirmed** | RTL generated heartbeat; no recorded visual confirmation | Observe and photograph separately if useful |
| JTAG / USB-Blaster | **Absent** | Host `jtagconfig -n`: no JTAG hardware; no USB-Blaster PnP device | Do not infer a chain or use an undocumented header |
| Configuration flash | **Blocked** | Never accessed | Requires exact programming topology and recovery plan |
| Persistent boot | **Blocked** | SD boot config not changed; staged RBF removed | Only consider after repeated volatile success and explicit design decision |
| Internal timing | **Historical positive slacks; one slow corner only** | Setup/hold/recovery/removal/min-pulse slacks were positive on Slow 1100 mV 100 C | Historical audits are invalidated for future loading by a separate scaler-width Critical Warning; repaired fit must be zero-critical |
| External I/O timing | **Unsigned** | 4 input and 50 output ports lack board delays | Must not quote this fit as external timing closure |
| Real Zhaozhou block execution | **Confirmed for v1 selected vectors** | Owner saw green signature bands; post-map retained CRC (133 ALUT), raster fill (1 ALUT), and direct packed multiplier (1 DSP); receipt records healthy load/rollback | Closes only the 16 named comparisons/signature in this wrapper; expanded host-readable/random/physical-mutant coverage remains open |
| Full Zhaozhou shell | **Not board-composed** | Virtual-pin shell fit succeeds separately | Replace 3,214-pin characterization boundary with real HPS/memory/video interfaces |

## Immediate sequence

1. Finish repair-only source/tests: deterministic high-Z/scaler build overlay,
   all-format Critical Warning rejection, complete manifest, and strict pinned
   loader/receipt validation.
2. After the separate 23:00 shell fit, take one repaired clean Quartus build;
   require zero Critical Warnings and all seven USER_IO output enables disabled.
3. Obtain independent review approval for the V2 audit/manifest before loading.
4. Run a separately named physical red/failure control via primary HPS-watchdog
   rollback, then a separately named green replay of the same fixed vectors.
5. Build the next gate as an HPS raw mailbox with nonce-selected inputs and
   independently host-readable raw results; do not promote the historical green
   signature into broader arithmetic, migration, or saving evidence.
