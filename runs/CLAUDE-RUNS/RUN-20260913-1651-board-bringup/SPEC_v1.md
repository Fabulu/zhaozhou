# SPEC v1: SuperStation FPGA board identification and safe bring-up

**Run ID:** RUN-20260913-1651
**Created:** 2026-09-13 16:51 UTC+02:00
**Status:** Active
**Previous Version:** N/A

---

## Objective

Establish an evidence-backed identity and safe-use baseline for the connected
SuperStation test hardware: exact board and FPGA marking, USB-Blaster/JTAG chain,
power state, oscillator, memory, pinout, documentation, and network/SSH access.
No bitstream is programmed and no FPGA pin is driven until the exact device,
I/O voltage standards, clock, reset, pin assignments, and a safe minimal design
have all been verified.

---

## Scope

**In Scope:**

- Read-only host, USB, JTAG, and local-network discovery.
- Board-document and repository archaeology.
- A committed bring-up record and, only after the safety gate is complete, a
  minimal non-invasive bring-up design in this dedicated checkout.
- Identifying how this board can run Zhaozhou specifications now, while keeping
  the later reference/debug FPGA board as a separate target.

**Out of Scope:**

- Programming FPGA configuration memory or volatile configuration.
- Driving, toggling, or probing board pins from an FPGA design before the safety
  gate is complete.
- Texture R0 Packet B work and the pinned shell-fit lane.
- Editing any shared build directory or any other checkout.

---

## Constraints

- Dedicated checkout/branch: `zhaozhou-board-bringup-20260913`, based on pushed
  hardware commit `158442b57b32f9bbf9cc9e241a88f43c2a660f7a`.
- Do not touch `zhaozhou-ceiling-lane-20260912`,
  `zhaozhou-shellfit-158442b5`, Packet-B files, or shared build directories.
- Use this Claude Code hardware session only; no HomeAI, alternate harness, or
  delegated implementation lane.
- Reserve USB-Blaster/programming access to this lane, but perform only
  read-only detection until the complete safety gate is documented.
- Avoid a large Quartus run during the other lane's pinned 23:00 shell fit.
- Independent review on 2026-09-13 places all future compile/load/programming on
  HOLD during repair. After 23:00, a repaired compile still requires committed
  V2 audit/complete manifest and explicit independent approval before loading.
- Historical physical results remain evidence but their RBFs/audits are not
  reusable load authorization.
- Commit and push only on `zhaozhou-board-bringup-20260913`.

---

## Don't Retry

- Do not infer the PCB, clock, memory, or I/O standards from the JTAG device ID
  alone. Match independent physical/documentary evidence.
- Do not treat SSH/network identity as proof of FPGA-board identity; the network
  endpoint may be a companion processor or another appliance.

---

## Open Questions

- What exact product/revision and FPGA package marking are physically present?
- What does the read-only JTAG chain report, including cable identity and IDCODE?
- Is the unit powered and stable, and which subsystem powers the JTAG interface?
- Which oscillators, memories, reset sources, and voltage rails are populated?
- Which authoritative schematic, pinout, BOM, manual, and source repository
  correspond to this exact revision?
- Is the board already present on the LAN, and if so under which IP, host key,
  hostname, and supported SSH authentication method?
