# SuperStation One capability matrix — 2026-09-13

**Board:** Retro Remake SuperStation One, exterior model marking
`RCSH-1001/1002`
**Volatile probe RBF:** SHA-256
`7e7b46f79685383dc85a057f88602154039e27cf4bea37fdfb911a55948ea9c0`
**Physical proof commit:** `2955a9e1`

A green row means the named question was exercised on the physical unit. It does
not generalise to a neighbouring interface.

| Capability | State | Physical evidence | Boundary / next test |
|---|---|---|---|
| Product identity | **Confirmed** | Owner read Retro Remake Hong Kong Limited, SuperStation One, `RCSH-1001/1002`, 5/9 V DC label | PCB revision and FPGA top marking remain unread |
| LAN discovery | **Confirmed** | Router DNS `mister.fritz.box`, `192.168.178.59` | DHCP address can change |
| SSH/HPS reachability | **Confirmed across FPGA loads** | SSH succeeded before, during, and after custom RBF; MiSTer PID and UTC captured each phase | Default password remains; key/password hardening not performed |
| FPGA device compatibility | **Confirmed for MiSTer RBF** | Audited `5CSEBA6U23I7` RBF configured and ran | Does not reveal physical ordering suffix |
| MiSTer framework | **Confirmed** | Declared core identity appeared as `Zhaozhou Board Bring-up` | Framework source is pinned; future updates need a new receipt |
| Volatile HPS loading | **Confirmed** | Staged hash matched; `/dev/MiSTer_cmd` load succeeded | JTAG and configuration flash are different paths and remain blocked |
| HDMI-visible video | **Confirmed by owner** | Owner reported visible Zhaozhou color bars | Display-reported output mode/photo and raw HDMI timing were not captured |
| Analog video | **Open** | No observation | Test each intended VGA/component/composite/S-Video mode separately |
| Core clock/reset | **Confirmed for probe** | Pinned PLL locked sufficiently to run raster/HPS protocol; reset/load/reload worked | Production GPU/video/audio frequencies and domain reset sequencing remain open |
| HPS↔FPGA bridges | **Confirmed across load** | `lwhps2fpga`, `hps2fpga`, `fpga2hps` stayed enabled | No production command/data transaction crossed them yet |
| Host-driven MENU rollback | **Confirmed twice** | MENU identity returned after both custom loads | Known `menu.rbf` hash is pinned in receipts |
| HPS rollback watchdog | **Deadline fires; FIFO action under repair** | Attempt 1 logged `watchdog-fired`, but its one-shot FIFO writer blocked; host fallback restored MENU | Retry/readiness script must return MENU and log write/result before this becomes proven |
| FPGA-side 128 MB SDRAM | **Open / intentionally untouched** | DQ output enables were disabled; no memory transaction | Run a separately audited address/data test before using it |
| HPS DDR from fabric | **Open / intentionally inactive** | All DDRAM request outputs held zero | Linux HPS memory working is not fabric-interface proof |
| Audio | **Open / intentionally silent** | Core outputs held zero | Test digital and analog paths separately at bounded level |
| Controller/input | **Open** | No controller transaction consumed | Test MiSTer input mapping and then native Zhaozhou snapshot path |
| User port / SNAC / GPIO | **Open / released** | `USER_OUT='1`; no intentional drive | Pin-specific tests require declared purpose and safe peripheral state |
| SD SPI from core | **Open** | Core requested high impedance; framework retains its standard mux behavior | No Zhaozhou SD transaction performed |
| LEDs | **Not owner-confirmed** | RTL generated heartbeat; no recorded visual confirmation | Observe and photograph separately if useful |
| JTAG / USB-Blaster | **Absent** | Host `jtagconfig -n`: no JTAG hardware; no USB-Blaster PnP device | Do not infer a chain or use an undocumented header |
| Configuration flash | **Blocked** | Never accessed | Requires exact programming topology and recovery plan |
| Persistent boot | **Blocked** | SD boot config not changed; staged RBF removed | Only consider after repeated volatile success and explicit design decision |
| Internal timing | **Confirmed for probe** | Positive setup/hold/recovery/removal/min-pulse slacks; zero illegal/unconstrained clocks | Applies only to this probe build |
| External I/O timing | **Unsigned** | 4 input and 50 output ports lack board delays | Must not quote this fit as external timing closure |
| Real Zhaozhou block execution | **Next** | Bring-up raster is new platform RTL, not the existing engine shell | Build a hardware spec runner from committed engine blocks |
| Full Zhaozhou shell | **Not board-composed** | Virtual-pin shell fit succeeds separately | Replace 3,214-pin characterization boundary with real HPS/memory/video interfaces |

## Immediate sequence

1. Fire-test the HPS rollback watchdog using the same already-audited RBF and a
   separately named receipt; host rollback remains the fallback.
2. Build a hardware spec runner containing real, already-verified Zhaozhou
   arithmetic/coverage/CRC blocks and report pass/fail visually.
3. Do not start another large Quartus job near the separate lane's pinned 23:00
   shell fit. Source/test work can proceed without touching that lane.
4. Integrate the full shell only after its real board interfaces and Packet-B
   composition are ready; do not load the virtual-pin characterization top.
