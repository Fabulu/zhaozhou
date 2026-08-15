# Zhaozhou design ledger — status dashboard

> GENERATED from `design/blocks.yml` + `design/ops.yml` by `npm run ledger:gen` — do not edit.
> Staleness is a CI failure: regenerated output must be byte-identical to the committed file (plan W2/R11).

Blocks: **87** (72 FPGA/rtl + 15 software) · Ops: **40** (28 ALU, 1 table, 6 sinks, 5 stamp modes) · Profiles: **5** (frozen five).

## Maturity matrix (charter §4 ladder)

| subsystem | SPECIFIED | REFERENCE_COMPLETE | UNIT_VERIFIED | RTL_VERIFIED | SYNTHESIZED | INTEGRATED | HARDWARE_PROVEN | blocked | total |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| audio | 1 | · | · | · | · | · | · | · | 1 |
| command | 3 | · | · | · | · | · | · | · | 3 |
| compositor | 5 | · | · | · | · | · | · | · | 5 |
| debug | 3 | · | · | · | · | · | · | · | 3 |
| field | 6 | · | · | · | · | · | · | · | 6 |
| forge | 2 | · | · | · | · | · | · | · | 2 |
| geometry | 10 | · | · | · | · | · | · | · | 10 |
| input | 1 | · | · | 2 | · | · | · | · | 3 |
| measure | 3 | · | · | · | · | · | · | · | 3 |
| memory | 4 | · | · | · | · | · | · | 1 | 4 |
| particles | 7 | · | · | · | · | · | · | · | 7 |
| platform | 3 | · | · | · | · | · | · | 3 | 3 |
| raster | 5 | · | · | · | · | · | · | · | 5 |
| surface | 2 | · | · | · | · | · | · | · | 2 |
| sw | 15 | · | · | · | · | · | · | 2 | 15 |
| terrain | 7 | · | · | · | · | · | · | · | 7 |
| texture | 4 | · | · | · | · | · | · | · | 4 |
| video | 4 | · | · | · | · | · | · | · | 4 |
| **all** | 85 | · | · | 2 | · | · | · | 6 | 87 |

## Evidence ledger (maturity > SPECIFIED)

| block | state | date | commit | evidence |
|---|---|---|---|---|
| INPUT.SNAPSHOT | REFERENCE_COMPLETE | 2026-08-15 | `8400661` | reference/include/zref/zref_input.hpp |
| INPUT.SNAPSHOT | UNIT_VERIFIED | 2026-08-15 | `bc94ced` | tests/input/input_snapshot_directed.cpp |
| INPUT.SNAPSHOT | RTL_VERIFIED | 2026-08-15 | `7ed046a` | tests/input/input_random.cpp |
| INPUT.RUMBLE | REFERENCE_COMPLETE | 2026-08-15 | `8400661` | reference/include/zref/zref_input.hpp |
| INPUT.RUMBLE | UNIT_VERIFIED | 2026-08-15 | `bc94ced` | tests/input/input_rumble_directed.cpp |
| INPUT.RUMBLE | RTL_VERIFIED | 2026-08-15 | `7ed046a` | tests/input/input_random.cpp |

## Budget groups vs §25 ceilings

Per-block percentage budgets are deliberately unfrozen until Phase 0 (charter §25: no absolute counts before board data); group membership is recorded from day one.

| §25 group | ceiling | rtl blocks | allocated ALM% |
|---|---:|---:|---:|
| platform | 14% | 14 | 0% |
| command_debug | 5% | 7 | 0% |
| field | 6% | 6 | 0% |
| geometry_mantle | 20% | 20 | 0% |
| tile | 30% | 11 | 0% |
| myriad_forge | 9% | 9 | 0% |
| twod_post | 6% | 5 | 0% |
| _reserve (untouchable)_ | 10% | — | — |

## Op × profile matrix (frozen five)

| op | class | E | W | F | M | S | cost | Field IR |
|---|---|:---:|:---:|:---:|:---:|:---:|---:|---|
| `FIELD.MOV` | alu | 1 | 1 | 1 | 1 | 1 | 1 | MOV |
| `FIELD.ADD` | alu | 1 | 1 | 1 | 1 | 1 | 1 | ADD |
| `FIELD.SUB` | alu | 1 | 1 | 1 | 1 | 1 | 1 | SUB |
| `FIELD.MUL` | alu | 1 | 1 | 1 | 1 | 1 | 1 | MUL |
| `FIELD.MAD` | alu | 1 | 1 | 1 | 1 | 1 | 1 | MAD |
| `FIELD.MIN` | alu | 1 | 1 | 1 | 1 | 1 | 1 | MIN |
| `FIELD.MAX` | alu | 1 | 1 | 1 | 1 | 1 | 1 | MAX |
| `FIELD.ABS` | alu | 1 | 1 | 1 | 1 | 1 | 1 | ABS |
| `FIELD.CLAMP` | alu | 1 | 1 | 1 | 1 | 1 | 1 | CLAMP |
| `FIELD.SELECT` | alu | 1 | 1 | 1 | 1 | 1 | 1 | SELECT |
| `FIELD.CMP` | alu | 1 | 1 | 1 | 1 | 1 | 1 | CMP |
| `FIELD.DOT2` | alu | 2 | 2 | 2 | 2 | · | 2 | DOT2 |
| `FIELD.DOT3` | alu | · | 2 | 2 | 2 | · | 2 | DOT3 |
| `FIELD.NORM.APPROX` | alu | · | 3 | 3 | 3 | · | 3 | NORMALIZE2 |
| `FIELD.SIN` | alu | 2 | 2 | 2 | 2 | · | 2 | SIN |
| `FIELD.COS` | alu | · | 2 | 2 | 2 | · | 2 | COS |
| `FIELD.CURVE` | alu | 1 | · | 1 | 1 | · | 1 | CURVE |
| `FIELD.NOISE2` | alu | 2 | · | 2 | · | 2 | 2 | 0x1C |
| `FIELD.LEN.APPROX` | alu | 2 | 2 | 2 | 2 | · | 2 | LEN2 |
| `FIELD.DIST.APPROX` | alu | 2 | 2 | 2 | 2 | · | 2 | DIST2 |
| `FIELD.SMOOTHSTEP` | alu | 2 | 2 | 2 | 2 | 2 | 2 | macro: FIELD.MUL + FIELD.MAD |
| `FIELD.RING` | alu | 2 | · | · | · | 2 | 2 | RING |
| `FIELD.RIDGE` | alu | 2 | · | · | · | · | 2 | RIDGE |
| `FIELD.SAMPLE.SPLINE` | alu | 2 | · | · | 2 | · | 2 | SPLINE |
| `FIELD.SAMPLE.CURVE` | alu | 1 | · | 1 | 1 | · | 1 | CURVE |
| `FIELD.ROT2` | alu | · | · | 2 | 2 | 2 | 2 | ROT2 |
| `FIELD.ROT3` | alu | · | 3 | · | 3 | · | 3 | ROT3 |
| `FIELD.DCURVE` | alu | 1 | 1 | 1 | 1 | · | 1 | 0x1D |
| `FIELD.RCP` | table | 2 | 2 | 2 | 2 | · | 2 | RCP |
| `FIELD.OUT.HEIGHT` | sink | 1 | · | · | · | · | 1 | OUT.HEIGHT |
| `FIELD.OUT.VELOCITY` | sink | 1 | · | · | · | · | 1 | OUT.VELOCITY |
| `FIELD.WRITE.MATERIAL` | sink | 2 | · | · | · | · | 2 | WRITE.MATERIAL |
| `FIELD.WRITE.NAV` | sink | 1 | · | · | · | · | 1 | WRITE.NAV |
| `FIELD.WRITE.TAG` | sink | · | · | · | · | 1 | 1 | WRITE.TAG |
| `FIELD.WRITE.HAZARD` | sink | 1 | · | · | · | · | 1 | WRITE.HAZARD |
| `FIELD.STAMP.MAX` | stamp_mode | · | · | · | · | 1 | 1 | STAMP.MAX |
| `FIELD.STAMP.ADD` | stamp_mode | · | · | · | · | 1 | 1 | STAMP.ADD |
| `FIELD.STAMP.SUB` | stamp_mode | · | · | · | · | 1 | 1 | STAMP.SUB |
| `FIELD.STAMP.REPLACE` | stamp_mode | · | · | · | · | 1 | 1 | STAMP.REPLACE |
| `FIELD.STAMP.AGE` | stamp_mode | · | · | · | · | 1 | 1 | STAMP.AGE |

Cell value = provisional cost_units (instruction slots feeding FORM §14 `costs.zcost`). Numeric opcodes exist where the plan froze them (NOISE2 0x1C, DCURVE 0x1D); other mnemonics are pinned to numbers by the ISA v1 table (spec/form/field-ir.md, W5).

## Blocked on hardware

| block | owner | why |
|---|---|---|
| SYS.PLL | ZH-000 | Absolute frequencies frozen post-Phase-0 (charter §25); Verilator lane uses a single conceptual clock. |
| SYS.RESET | ZH-000 | Fanout to every domain's reset; only the SYS.CDC edge is drawn in the schematic. |
| SYS.CDC | ZH-000 | leaf consumer of the reset tree; instantiated by every bridge block (async_bridge: true below). |
| MEM.SDRAM | ZH-004 | Timings are board data (ZH-004); simulation model is cycle-approximate until board_truth.json exists. |
| SW.TOOLS.REPORT | ZH-002 | Contract filled (Phase-1-active scope note). Parser untested until Quartus exists (plan §4); V5 activates at SYNTHESIZED. |
| SW.TOOLS.BOARDPROBE | ZH-003 | Contract filled (Phase-1-active scope note). No architecture constant is frozen from assumptions while this is blocked (§23 Phase-0 gate). |

Rule: `blocked_on: hardware` blocks never advance from SPECIFIED regardless of evidence, until the orchestrator clears the hardware lane (plan §4).

## Deferred / cut order (§26)

| cut order | block | deferred |
|---:|---|---|
| 1 | POST.ECHO | yes |
| 2 | TEXTURE.AUX | no |
| 4 | TWOD.PLANE | no |
| 5 | FIELD.SEQ.WARP | yes |
| 5 | GEOM.WARP | yes |
| 6 | POST.GATHER | no |

Deferred without a cut slot: INPUT.SNAC.

## §25 counter coverage

§25 minimum counters wired to at least one rtl block: **32/32**.
