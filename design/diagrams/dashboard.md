# Zhaozhou design ledger — status dashboard

> GENERATED from `design/blocks.yml` + `design/ops.yml` by `npm run ledger:gen` — do not edit.
> Staleness is a CI failure: regenerated output must be byte-identical to the committed file (plan W2/R11).

Blocks: **88** (73 FPGA/rtl + 15 software) · Ops: **40** (28 ALU, 1 table, 6 sinks, 5 stamp modes) · Profiles: **5** (frozen five).

## Maturity matrix (charter §4 ladder)

| subsystem | SPECIFIED | REFERENCE_COMPLETE | UNIT_VERIFIED | RTL_VERIFIED | SYNTHESIZED | INTEGRATED | HARDWARE_PROVEN | blocked | total |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| audio | · | · | · | 1 | · | · | · | · | 1 |
| command | 1 | · | 2 | · | · | · | · | · | 3 |
| compositor | 5 | · | · | · | · | · | · | · | 5 |
| debug | 1 | · | 2 | · | · | · | · | · | 3 |
| field | 6 | · | · | · | · | · | · | · | 6 |
| forge | 2 | · | · | · | · | · | · | · | 2 |
| geometry | 11 | · | · | · | · | · | · | · | 11 |
| input | 1 | · | · | 2 | · | · | · | · | 3 |
| measure | 3 | · | · | · | · | · | · | · | 3 |
| memory | 1 | · | · | 3 | · | · | · | 1 | 4 |
| particles | 7 | · | · | · | · | · | · | · | 7 |
| platform | 3 | · | · | · | · | · | · | 3 | 3 |
| raster | 5 | · | · | · | · | · | · | · | 5 |
| surface | 2 | · | · | · | · | · | · | · | 2 |
| sw | 9 | 3 | 3 | · | · | · | · | 2 | 15 |
| terrain | 7 | · | · | · | · | · | · | · | 7 |
| texture | 4 | · | · | · | · | · | · | · | 4 |
| video | · | · | 4 | · | · | · | · | · | 4 |
| **all** | 68 | 3 | 11 | 6 | · | · | · | 6 | 88 |

## Evidence ledger (maturity > SPECIFIED)

| block | state | date | commit | evidence |
|---|---|---|---|---|
| CMD.DMA | REFERENCE_COMPLETE | 2026-08-16 | `768ce1a` | reference/include/zref/zref_cmd2.hpp |
| CMD.DMA | UNIT_VERIFIED | 2026-08-15 | `b64afe2` | tests/command/cmd_dma_directed.cpp |
| CMD.SCHEDULER | REFERENCE_COMPLETE | 2026-08-15 | `38f9b96` | reference/include/zref/zref_cmd2.hpp |
| CMD.SCHEDULER | UNIT_VERIFIED | 2026-08-16 | `768ce1a` | tests/command/cmd_scheduler_directed.cpp |
| MEM.SDRAM | SPECIFIED | 2026-08-15 | `6bcc4e9` | BANKED (blocked_on: hardware — never advances from SPECIFIED, plan D2): synthesizable core zhao_sdram_ctrl.sv + behavioural model sim/models/zhao_sdram_model.sv verified against the frozen sim profile — ctest mem_sdram_directed (exact grant-to-grant spans, refresh steals = 12 per preempted refresh, DQM-masked partial bursts, bank-conflict accounting, model timing-clean) and the three-way differential mem_random (1k/100k, oracle mismatches 0); verilator -Wall lint clean; formal mem_sdram_refresh_bound ready (SKIPped on the oss-cad-suite yosys SV-frontend gap, see tests/formal/mem_formal_lane.cmake.in). ZH-004 obligations unfreeze the profile via zhao_sdram_params_pkg.sv. |
| MEM.VRAM.ARBITER | REFERENCE_COMPLETE | 2026-08-15 | `6bcc4e9` | reference/include/zref/zref_mem.hpp |
| MEM.VRAM.ARBITER | UNIT_VERIFIED | 2026-08-15 | `6bcc4e9` | tests/memory/vram_arbiter_directed.cpp |
| MEM.VRAM.ARBITER | RTL_VERIFIED | 2026-08-15 | `6bcc4e9` | tests/memory/mem_random.cpp |
| MEM.VRAM.ARBITER | RTL_VERIFIED | 2026-08-16 | `9d49806` | tests/formal/mem_vram_arbiter_liveness.sby |
| MEM.HPS.BRIDGE | REFERENCE_COMPLETE | 2026-08-15 | `6bcc4e9` | reference/include/zref/zref_mem.hpp |
| MEM.HPS.BRIDGE | UNIT_VERIFIED | 2026-08-15 | `6bcc4e9` | tests/memory/hps_bridge_directed.cpp |
| MEM.HPS.BRIDGE | RTL_VERIFIED | 2026-08-15 | `b8db7e8` | tests/memory/hps_bridge_random.cpp |
| MEM.GUARD | REFERENCE_COMPLETE | 2026-08-15 | `6bcc4e9` | reference/include/zref/zref_mem.hpp |
| MEM.GUARD | UNIT_VERIFIED | 2026-08-15 | `6bcc4e9` | tests/memory/mem_guard_directed.cpp |
| MEM.GUARD | RTL_VERIFIED | 2026-08-15 | `b8db7e8` | tests/formal/formal_mem_guard.sv |
| INPUT.SNAPSHOT | REFERENCE_COMPLETE | 2026-08-15 | `8400661` | reference/include/zref/zref_input.hpp |
| INPUT.SNAPSHOT | UNIT_VERIFIED | 2026-08-15 | `bc94ced` | tests/input/input_snapshot_directed.cpp |
| INPUT.SNAPSHOT | RTL_VERIFIED | 2026-08-15 | `7ed046a` | tests/input/input_random.cpp |
| INPUT.RUMBLE | REFERENCE_COMPLETE | 2026-08-15 | `8400661` | reference/include/zref/zref_input.hpp |
| INPUT.RUMBLE | UNIT_VERIFIED | 2026-08-15 | `bc94ced` | tests/input/input_rumble_directed.cpp |
| INPUT.RUMBLE | RTL_VERIFIED | 2026-08-15 | `7ed046a` | tests/input/input_random.cpp |
| AUDIO.FIFO | REFERENCE_COMPLETE | 2026-08-15 | `9e813e0` | reference/src/zref_audio.cpp |
| AUDIO.FIFO | UNIT_VERIFIED | 2026-08-15 | `a3cd94a` | tests/audio/audio_fifo_directed.cpp |
| AUDIO.FIFO | RTL_VERIFIED | 2026-08-15 | `a3cd94a` | tests/audio/audio_fifo_random.cpp |
| AUDIO.FIFO | RTL_VERIFIED | 2026-08-15 | `a3cd94a` | tests/formal/audio_fifo_bounds.sby |
| VIDEO.MODE | UNIT_VERIFIED | 2026-08-16 | `a4ea5d9` | tests/video/video_mode_directed.cpp |
| VIDEO.MODE | REFERENCE_COMPLETE | 2026-08-16 | `0b8c71c` | reference/include/zref/zref_video.hpp |
| VIDEO.SCANOUT | UNIT_VERIFIED | 2026-08-16 | `a4ea5d9` | tests/video/video_scanout_directed.cpp |
| VIDEO.SCANOUT | REFERENCE_COMPLETE | 2026-08-16 | `0b8c71c` | reference/include/zref/zref_video.hpp |
| VIDEO.SCALER | UNIT_VERIFIED | 2026-08-16 | `a4ea5d9` | tests/video/video_scaler_directed.cpp |
| VIDEO.SCALER | REFERENCE_COMPLETE | 2026-08-16 | `0b8c71c` | reference/include/zref/zref_video.hpp |
| VIDEO.FRAMECTL | UNIT_VERIFIED | 2026-08-16 | `a4ea5d9` | tests/video/video_framectl_directed.cpp |
| VIDEO.FRAMECTL | REFERENCE_COMPLETE | 2026-08-16 | `0b8c71c` | reference/include/zref/zref_video.hpp |
| DEBUG.COUNTERS | REFERENCE_COMPLETE | 2026-08-15 | `38f9b96` | reference/include/zref/zref_cmd2.hpp |
| DEBUG.COUNTERS | UNIT_VERIFIED | 2026-08-15 | `b64afe2` | tests/debug/debug_counters_directed.cpp |
| DEBUG.CRC | REFERENCE_COMPLETE | 2026-08-16 | `768ce1a` | reference/include/zref/zref_cmd2.hpp |
| DEBUG.CRC | UNIT_VERIFIED | 2026-08-16 | `768ce1a` | tests/debug/debug_crc_directed.cpp |
| SW.MIXER | REFERENCE_COMPLETE | 2026-08-15 | `9e813e0` | tests/audio/mixer_tone_directed.cpp |
| SW.ZREF | REFERENCE_COMPLETE | 2026-08-14 | `7279493` | tests/unit/test_fixp.cpp |
| SW.ZREF | REFERENCE_COMPLETE | 2026-08-14 | `f0edffa` | reference/include/zref/generated/zref_tables.hpp |
| SW.ZREF | REFERENCE_COMPLETE | 2026-08-14 | `9d8dc79` | reference/src/zref_frame.cpp |
| SW.ZREF | REFERENCE_COMPLETE | 2026-08-14 | `4131647` | tests/differential/test_empty_frame_replay.cpp |
| SW.ZREF | REFERENCE_COMPLETE | 2026-08-14 | `e8d652e` | reference/src/zfield/zfield_interpret.cpp |
| SW.ZREF | REFERENCE_COMPLETE | 2026-08-14 | `5db7844` | reference/src/zref.cpp |
| SW.FIELDIR | REFERENCE_COMPLETE | 2026-08-14 | `500965d` | spec/form/field-ir.md |
| SW.FIELDIR | REFERENCE_COMPLETE | 2026-08-14 | `681a0b6` | compiler/src/field_ir |
| SW.FIELDIR | REFERENCE_COMPLETE | 2026-08-14 | `e8d652e` | captures/golden/field/crater_ring.zvec |
| SW.FIELDIR | REFERENCE_COMPLETE | 2026-08-14 | `681a0b6` | tests/fuzz/corpus/field |
| SW.TOOLS.LEDGER | REFERENCE_COMPLETE | 2026-08-14 | `f036f75` | tools/ledger/src/rules.ts |
| SW.TOOLS.LEDGER | REFERENCE_COMPLETE | 2026-08-14 | `8bdeac8` | tools/ledger/src/gen/dashboard.ts |
| SW.TOOLS.LEDGER | UNIT_VERIFIED | 2026-08-14 | `f036f75` | tools/ledger/src/test/rules.test.ts |
| SW.TOOLS.ABIDOC | REFERENCE_COMPLETE | 2026-08-14 | `562787f` | tools/abi-gen/src/main.ts |
| SW.TOOLS.ABIDOC | REFERENCE_COMPLETE | 2026-08-14 | `0383ed1` | tests/abi/golden/frame_minimal.bin |
| SW.TOOLS.ABIDOC | UNIT_VERIFIED | 2026-08-14 | `4493b9b` | tools/abi-gen/test/abi_gen.test.ts |
| SW.TOOLS.CAPTURE | REFERENCE_COMPLETE | 2026-08-14 | `9d8dc79` | tools/capture/zhao_capture.cpp |
| SW.TOOLS.CAPTURE | REFERENCE_COMPLETE | 2026-08-14 | `9d8dc79` | tests/unit/test_zcap_roundtrip.cpp |
| SW.TOOLS.CAPTURE | UNIT_VERIFIED | 2026-08-14 | `0383ed1` | tests/abi/golden/zcap_minimal.zcap |

## Budget groups vs §25 ceilings

Per-block percentage budgets are deliberately unfrozen until Phase 0 (charter §25: no absolute counts before board data); group membership is recorded from day one.

| §25 group | ceiling | rtl blocks | allocated ALM% |
|---|---:|---:|---:|
| platform | 14% | 14 | 0% |
| command_debug | 5% | 7 | 0% |
| field | 6% | 6 | 0% |
| geometry_mantle | 20% | 21 | 0% |
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
