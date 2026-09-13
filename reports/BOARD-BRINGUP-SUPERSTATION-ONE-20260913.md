# SuperStation One board bring-up inventory — 2026-09-13

**Run:** `RUN-20260913-1651-board-bringup`
**Branch:** `zhaozhou-board-bringup-20260913`
**Base:** `158442b57b32f9bbf9cc9e241a88f43c2a660f7a`
**Safety state:** **HOLD — no Zhaozhou bitstream may be loaded yet**

This record keeps live observations, published facts, and compatibility
assumptions separate. A standard MiSTer core operating on the unit is valuable
compatibility evidence; it is not a substitute for reading the owner's physical
PCB and component markings.

## Target decision

The connected **SuperStation One** is Zhaozhou's current FPGA target for running
specification tests. A later FPGA board, expected to be closer to the reference
specification and to expose more debug/JTAG facilities, is a separate target. Its
clock, memory, pinout, debug, and programming assumptions must not leak into this
one.

## Owner-read exterior marking

The owner read the following directly from the unit/label on 2026-09-13:

```text
SuperStation One
Retro Remake Hong Kong Limited
Model RCSH-1001/1002
5 V / 9 V DC
EXT: 5 V 3 A / 9 V 3 A
```

This closes the product-family and rated-input questions. The combined
`RCSH-1001/1002` marking is recorded literally; it does not by itself select one
of those two model suffixes, identify the PCB revision, or report the voltage
currently negotiated with the USB-PD supply.

## Live unit — confirmed read-only observations

Observed over the local network on 2026-09-13. No FPGA configuration or board
pin was changed.

| Subject | Observation | Meaning / limit |
|---|---|---|
| Network name | Router DNS answers `mister.fritz.box` for `192.168.178.59`; `mister` resolves locally | DHCP address; it may change unless reserved |
| Ethernet | `eth0`, 1 Gbit/s, full duplex, carrier present; MAC `ce:cd:87:14:8d:44` | MAC is locally administered, so it is not vendor evidence |
| SSH | OpenSSH 8.6 on TCP 22; password login as the standard MiSTer root account succeeded | Read-only shell access is established |
| SSH ED25519 host key | `SHA256:FqNJOsj3FLUoMQxgn+cqGoXvVfENmVK4QFoSCMKl2lU` | Pin this fingerprint when the address changes |
| SSH ECDSA host key | `SHA256:H+ACi6DPoz55a51/Ym3DrUNpHU/OEaFoAl5DqerRhko` | Secondary host-key evidence |
| SSH RSA host key | `SHA256:eDrKAiW7GupcPO/m5f3Rwuo2lJeOPemY7do8nomM5bY` | Secondary host-key evidence |
| OS | `Linux MiSTer 5.15.1-MiSTer`, Buildroot 2021.02.4 | Standard MiSTer software environment |
| Live core | Official-style `NES_20260823.rbf` launched by `/media/fat/MiSTer` | Existing MiSTer RBFs configure and operate on the unit |
| Device-tree model | `Terasic DE10-nano` | Compatibility device tree; **not** proof that the PCB is a Terasic DE10-Nano |
| Device-tree compatibility | `altr,socfpga-cyclone5`, `altr,socfpga` | Confirms the Cyclone V SoC HPS family |
| CPU | Two Arm Cortex-A9 cores (`ARM part 0xc09`) | Matches Cyclone V SE/SX/ST SoC HPS family; does not identify package/speed grade |
| HPS silicon revision register | System Manager `siliconid1` at `0xffd08000` read `0x00000003`; `siliconid2` read zero | Read-only revision evidence; these registers do not encode the exact FPGA ordering code or a unique ID |
| FPGA manager | `Altera SOCFPGA FPGA Manager`, state `operating` | FPGA fabric is configured now; it does not reveal the physical package marking |
| HPS clock input | Device tree reports `osc1 = 0x017d7840 = 25,000,000 Hz` | Confirms the firmware's HPS reference-clock contract, not the oscillator part marking |
| HPS DDR map | Device tree declares `0x40000000` bytes (1 GiB). Boot args expose 511 MiB to Linux and reserve 513 MiB: `mem=511M memmap=513M$511M`; Linux reports 504,096 KiB after overhead | Confirms the live 1 GiB address-map contract. DRAM manufacturer/part marking remains unverified |
| FPGA-side SDRAM | Not directly identified from Linux | Official product claim is separate: 128 MB BGA SDR SDRAM |
| SD storage | `mmcblk0`, 238 GiB SDXC, with current two-partition MiSTer layout | Owner media, not board BOM evidence |
| USB/dock topology | Internal hubs; Realtek `0bda:c820` Wi-Fi/Bluetooth; CH341 serial; TinyUSB IR keyboard; JMicron NVMe bridge; Initio optical-drive bridge | Strong live evidence that the SuperDock is attached; owner confirmation remains preferable |
| Power state | HPS Linux responds, Ethernet carrier is stable, and FPGA manager says `operating` | Unit is powered and functioning. Input PD contract and internal rail voltages were **not** measured |
| Boot image hashes | `uboot.img`: `e2d46cf9fe1ec40ca2c9c7409870249f267e06f70e5736dc6d30b4e21fe62a64`; `zImage_dtb`: `b6d9754ecfba0f402fcbe922d75a66b070a326de9fd9b13a6b2c9c216b5146fa` | Reproducible software identity, not board identity |

### SSH entry point

```text
ssh root@192.168.178.59
```

`mister` also resolves on the present LAN; `mister.local` did not resolve from
Windows. Do not commit or paste the root password into project files. The unit
still accepts the standard MiSTer password; key installation and password
rotation are prudent later, but are state-changing administration and were not
performed during identification.

## USB-Blaster / JTAG chain

Host command:

```text
C:\intelFPGA_lite\17.0\quartus\bin64\jtagconfig.exe -n
```

Result:

```text
No JTAG hardware available
```

Windows PnP enumeration found no present Intel/Altera USB-Blaster/JTAG device.
The unit's own USB tree also contains no USB-Blaster. Therefore there is **no
verified JTAG chain in this setup** and no cable/device IDCODE to report. Do not
confuse the SSH/HPS path with JTAG. Public SuperStation documentation found so
far does not define an exposed JTAG connector or safe cable hookup.

## Published board facts and compatibility contract

### Product-level facts

Retro Remake's [SuperStation One product page](https://retroremake.co/pages/superstation%E1%B5%92%E2%81%BF%E1%B5%89)
states:

- Cyclone V FPGA (no exact ordering code on that page);
- 128 MB BGA SDRAM;
- 24-bit ADV7125 analog-video DAC;
- HDMI, VGA, DIN-10, composite/component, analog/TOSLINK audio;
- Ethernet, Wi-Fi/Bluetooth, NFC, three USB-A ports, microSD, PS1 SNAC ports,
  and SuperDock expansion;
- USB-C power.

The maintained [SuperStation documentation](https://github.com/Takiiiiiiii/SuperStation-Documentation)
says 9 V / 3 A USB-PD is recommended. Its troubleshooting section accepts a
suitable 5 V / 3 A or 9 V / 3 A supply, and its setup text also describes 5 V
or 12 V as supported when negotiated through USB-PD. Those are product
requirements, **not a measurement of the adapter or rails currently attached**.
The same documentation says the unit runs MiSTer by default.

The official [SuperStation SD-card installer](https://github.com/Retro-Remake/SuperStation-SD-Card-Installer/releases/tag/1.2)
provides board software images but no schematic, BOM, or engineering pinout.

### Exact FPGA target used by the compatible framework

The [openfpgaOS compatibility README](https://github.com/openfpgaOS/openfpgaCore/blob/main/README.md)
explicitly groups SuperStation One with its MiSTer target and names
`5CSEBA6U23I7`, with an SDR SDRAM module required. This is strong target evidence,
but it is still source compatibility rather than a reading of the owner's chip.

The canonical MiSTer template at commit
[`3ea1134cf05d62c2b1db30362277a823d739ced2`](https://github.com/MiSTer-devel/Template_MiSTer/tree/3ea1134cf05d62c2b1db30362277a823d739ced2)
defines in
[`sys/sys.tcl`](https://github.com/MiSTer-devel/Template_MiSTer/blob/3ea1134cf05d62c2b1db30362277a823d739ced2/sys/sys.tcl):

```text
DEVICE                   5CSEBA6U23I7
DEVICE_FILTER_PACKAGE    UFBGA
DEVICE_FILTER_PIN_COUNT  672
DEVICE_FILTER_SPEED_GRADE 7
```

Intel/Altera's authoritative package resources are the
[5CSEBA6 pin-information workbook](https://docs.altera.com/v/u/resources/656491/pin-information-for-the-cyclone-v-5cseba6-device-xls-format)
and the
[5CSEBA6U23 672-pin BSDL model](https://www.intel.com/content/www/us/en/content-details/651577/cyclone-v-ieee-1149-1-compliant-bsdl-model-for-5cseba6u23-672-pin-fineline-bga.html).
They describe the chip/package, not the SuperStation PCB nets.

**Current target judgement:** retain `5CSEBA6U23I7` as the SuperStation build
target. It is no longer merely an arbitrary capacity proxy: it is the device
contract of the MiSTer-compatible software currently operating on the unit.
The physical top marking (including any ordering suffix) is nevertheless still
required before the safety gate can close.

## Clock, reset, I/O voltage, and pin assignments

The canonical MiSTer
[`sys/sys_top.sdc`](https://github.com/MiSTer-devel/Template_MiSTer/blob/3ea1134cf05d62c2b1db30362277a823d739ced2/sys/sys_top.sdc)
constrains three FPGA inputs to 50.000 MHz:

| Logical input | FPGA pin | I/O standard | Framework use |
|---|---:|---|---|
| `FPGA_CLK1_50` | `V11` | 3.3-V LVTTL | HDMI PLL/reference path |
| `FPGA_CLK2_50` | `Y13` | 3.3-V LVTTL | MiSTer framework and core `CLK_50M` |
| `FPGA_CLK3_50` | `E11` | 3.3-V LVTTL | Audio PLL/reference path |

Live software independently reports the HPS `osc1` input as 25 MHz. The
physical FPGA oscillator manufacturer, tolerance, and exact PCB clock routing
remain unverified.

The same `sys.tcl` is the complete FPGA-facing MiSTer pin contract. Its explicit
soft-I/O standards are 3.3-V LVTTL for the clock inputs, ADC signals, HDMI
parallel control/data, framework I/O, user I/O, SD SPI, FPGA-side SDRAM, keys,
switches, and LEDs. Notable assignments include:

- keys: `KEY[0]=AH17`, `KEY[1]=AH16`;
- SDRAM clock/control: `SDRAM_CLK=AD20`, `CKE=AG10`, `nWE=AA19`,
  `nCAS=AA18`, `nCS=Y18`, `nRAS=W14`;
- SDRAM data/address: the full 16-bit DQ, 13-bit address, two bank-address,
  and mask pin list is in the pinned `sys.tcl` above.

The framework's `sys_top.v` feeds `FPGA_CLK2_50` to a core as `CLK_50M`. Core
reset is generated by the MiSTer HPS/framework reset handshake and presented as
`RESET`; motherboard keys are debounced separately. This is the currently
operating compatibility contract, **not a published SuperStation motherboard
schematic**.

## Documentation availability

Checked on 2026-09-13:

- Retro Remake's public GitHub organization contains the SuperStation installer,
  distributions, Console Mode, and MiSTer-Pi hardware repository.
- No public SuperStation motherboard schematic, PCB source, Gerbers, BOM,
  netlist, or revision-specific electrical pinout was found.
- The community SuperStation documentation is an operating guide, not an
  electrical design package.
- A public development photograph in the
  [SuperStation development interview](https://readonlymemo.com/superstation-one-development-history-making-of-taki-udon-interview/)
  shows an **unpopulated reference PCB** silked `SuperStation one Founders
  Edition / v1.2`. It does not prove the revision of the owner's populated unit;
  the article also discusses later revision `1.2.1`.

Therefore the canonical MiSTer constraints are presently the best reproducible
FPGA-facing pinout, but they do not close the revision-specific board-electrical
question on their own.

## Safety gate

| Required item | State | Evidence needed to close |
|---|---|---|
| Exact product | **Confirmed exterior label** | SuperStation One, Retro Remake Hong Kong Limited, model marking `RCSH-1001/1002` |
| Exact FPGA die target | **Strong** | MiSTer/OpenFPGA target plus currently operating official core |
| Exact FPGA package/speed/top marking | **Open** | Read the physical package marking; do not infer trailing ordering suffix |
| PCB revision | **Open** | PCB silk or revision label on this unit |
| JTAG cable and chain | **Absent** | A physically connected, documented cable/header and read-only IDCODE enumeration |
| Current power input | **Rated input confirmed; live negotiation open** | Exterior marking allows 5 V / 3 A or 9 V / 3 A; present USB-PD contract and rail values remain unmeasured |
| Power state | **Confirmed operating** | HPS, FPGA manager, and Ethernet are live |
| HPS clock | **Confirmed contract** | 25 MHz from live device tree |
| FPGA clocks | **Strong compatibility contract** | 50 MHz x3, canonical MiSTer constraints; physical oscillator marking/routing still open |
| HPS DDR map | **Confirmed contract** | 1 GiB DT map; exact memory component marking open |
| FPGA SDRAM | **Published, not live-probed** | 128 MB BGA SDR SDRAM; exact part/revision and a safe memory test later |
| FPGA I/O standards/pinout | **Strong compatibility contract** | Pinned MiSTer constraints; revision-specific SSOne schematic unavailable |
| Reset contract | **Framework-confirmed** | HPS/MiSTer generated core reset; physical button/reset mapping still open |
| Safe minimal design | **Not yet approved** | Must explicitly leave every nonessential pin input/tri-stated, use the pinned framework/device, and pass review before any RBF load |

## Next safe actions

1. If the PCB later becomes safely accessible for another reason, photograph the
   PCB silk and FPGA top marking; do not open the unit or remove its heatsink
   merely to satisfy this record.
2. Pin a DHCP reservation for the confirmed MAC or continue using router DNS;
   this is a router state change and was not done automatically.
3. Prepare a **build-only** MiSTer-framework probe whose default state drives no
   user, SDRAM, video, audio, LED, or expansion output. Review the final Quartus
   pin report and unused-pin policy before producing a candidate RBF.
4. Do not load that RBF until the physical marking/revision and power/clock/pin
   gate above is explicitly closed.
5. After first load is authorised, use MiSTer's volatile HPS FPGA-manager path,
   not configuration-flash programming, and establish a rollback to the known
   menu core before loading anything.
