# RATIFICATION — the destination is fabricated silicon; MiSTer is a lane, not the end goal

*Orchestrator decision, 2026-08-16, from the owner's direct statement: "the site says this is aimed at the mister fpga. While that's true in a way, the end goal is actually printing the chips and actually making this a physical console. While still having the Mister FPGA core work. Make sure you make that possible as you design the whole hardware. For the game later, we also want to port it to steam… if you can make a choice that facilitates that vs not, you can weigh that in."*

## What this changes

**Nothing that exists is invalidated. Everything from here is oriented differently.**

The charter and all ratified specs target an FPGA (Cyclone V via the MiSTer framework). That remains: the MiSTer core must keep working as a first-class lane, and the Phase-0 hardware-probe obligations are unchanged. What changes is the *destination*: the machine is designed to be **fabricatable as a physical console**, with the FPGA as the proving ground, not the terminus.

**Consequences for design decisions, effective now:**

1. **No MiSTer-only primitives.** Anything the RTL uses must have a plausible ASIC path. Synchronous resets (already the subset's habit), vendor-neutral RTL per the charter's conservative SystemVerilog, no vendor IP cores where a portable equivalent is affordable. MiSTer framework integration stays quarantined where the charter already puts it (`sys/` untouched, template fork) so the core itself carries no framework dependency inward.
2. **Memory interfaces get an abstraction boundary.** The SDRAM-facing controller is already cycle-exact and oracle-mirrored; treat the *port contract* as the stable seam so an ASIC-period memory controller can replace the PHY-facing side without touching the machine above it. The ZH-004 bank-mapping ratification already models this style: one constant is the retuning knob, the law above it is invariant.
3. **Clocking and timing closures must not assume FPGA conveniences** (abundant PLLs, vendor DDR IP, on-fabric RAM of arbitrary aspect ratio). Where a block needs a RAM, its contract should state shape/ports/width so a compiled SRAM macro maps later.
4. **The zref reference model doubles as the Steam foundation.** The charter's §19 law (byte-identical ABI across C++/TS/SV) already forces game-facing logic to be platform-agnostic. Rule of thumb for future choices: game/sim logic lives against the reference ABI, never against RTL specifics — then the same C++ compiles for a PC target. When two designs are otherwise equal, prefer the one that keeps the sim/reel/capture stack buildable as a standalone executable.
5. **Site copy must state this honestly.** The current page says "being built for FPGA" and describes a MiSTer board. Amend to state the physical-console destination with the FPGA lane as the proving ground. (Site work must pass the copy gate; no em dashes.)

## What this does NOT change

- The §26 refusals (they were right for an FPGA budget and are doubly right for a fabricable one).
- The maturity ladder, the evidence discipline, the capture law. If anything, HARDWARE_PROVEN reads differently now: proven on the FPGA is the gate *toward* silicon, not the end of the road.
- The MiSTer core's obligations — it stays working throughout.
- Priority order: terrain effects and character LOD/deformation remain highest (owner's standing order, same session).

## Recording

Persisted to memory (`zhaozhou-hardware-destination`). The site-copy amendment and a charter-scope note (a short addendum, not a rewrite: the charter's product definition gains the silicon destination sentence) go to the next site/spec agent. No spec file changes today — this ratification is the reference until those edits land.
