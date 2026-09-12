# D3 shell fit-top split architecture

**Date:** 2026-09-12  
**Scope:** architecture only; no RTL, fit, run-log, commit, or board work  
**Decision:** keep `zhao_shell_top` byte-for-byte as the real shell composition and add one generated, fit-only `zhao_shell_fit_top`. Do not rename the existing shell, do not move the simulation composition, and do not invent `zhao_board_top` before board/framework facts exist.

D3 is measurement architecture. It is not an ALM-saving change.

## 1. Checked facts and corrected claims

### D3 is still absent

The evidence is direct:

* The docket still describes D3 as an outstanding split, not a completed change (`reports/DOCKET.md:263-268`).
* The only current shell composition module is `zhao_shell_top`, whose wide boundary begins at `fpga/rtl/common/zhao_shell_top.sv:96`.
* The dedicated Quartus project still selects `zhao_shell_top`, not a fit wrapper (`fpga/quartus/shell_fit/zhao_shell_fit.qsf:5-9`).
* That project still applies `VIRTUAL_PIN ON` to the shell boundary (`fpga/quartus/shell_fit/zhao_shell_fit.qsf:214-233`).
* A repository search found the names `zhao_shell_fit_top`, `zhao_shell_sim_top`, and `zhao_shell_core` only in proposals and run notes, not in an RTL module declaration.

The fit project is also stale relative to the current boundary. The shell now exposes the packed `geom_guard_req_i`/`geom_guard_rsp_o` types and three geometry return-beat ports (`fpga/rtl/common/zhao_shell_top.sv:256-264`), while the hand-maintained virtual-pin list ends without them. This is precisely the kind of port drift D3 must make impossible.

### The 3,214 and 1,608 numbers are genuine, historical Quartus output

The archived raw fitter summary says:

* `Logic utilization (in ALMs) : 12,569`;
* `ALMs containing virtual pins : 1,608`;
* `Total virtual pins : 3,214`.

Those are verbatim at `reports/composed/renderer-f8c2b32-20260831T023107Z/synthesis/zhao_shell_fit.fit.summary:8-13`. The adjacent result identifies commit `f8c2b32`, device `5CSEBA6U23I7`, and Quartus Prime Lite 17.0.2 (`reports/composed/renderer-f8c2b32-20260831T023107Z/RESULT.md:8-12`). The contemporaneous log records that `run_shell_fit.ps1` measured `git archive HEAD`, rather than dirty worktree bytes (`runs/CLAUDE-RUNS/RUN-20260831-0213-renderer-attribute-path/TASK_LOG.md:124-126`). That is adequate provenance for a **committed historical specimen**.

A later receipt at commit `b3bd69b7...` independently reports 3,214 virtual pins, with 12,707 ALMs, but does not preserve the “ALMs containing virtual pins” field (`reports/synthesis/zhao_shell_fit.json:4-30`). Thus:

* **3,214 virtual-pin bits:** reproduced in two committed historical artifacts;
* **1,608 ALMs containing virtual pins:** reproduced directly in one committed raw fitter artifact;
* **neither number:** a measurement of the current RTL.

The old interpretation that subtracting 1,608 from 12,569 gives an “honest” lower shell size (`reports/composed/renderer-f8c2b32-20260831T023107Z/RESULT.md:28-31`) is wrong. ALMs are packing units. Removing virtual pins changes packing, placement, duplication, and sometimes surrounding logic; “ALMs containing virtual pins” is not a linearly subtractable wrapper cost.

### Current source-level boundary census

A declaration audit of the current module gives 154 named ports: 59 inputs and 95 outputs. Packed and unpacked widths total **3,386 bits**, comprising 1,457 input bits and 1,929 output bits. Of the input bits, four are clocks/reset and 1,453 are pseudo-input data/control bits.

The increase from the historical 3,214 is exactly the current 172-bit geometry memory seam:

* `zhao_guard_req_t`: 103 input bits;
* `zhao_guard_rsp_t`: 3 output bits;
* geometry return beat: 66 output bits.

The request type is 1 valid + 1 write + 3 client + 27 address + 7 length + 64 byte enables (`fpga/rtl/common/zhao_pkg.sv:307-320`); the response is three bits (`fpga/rtl/common/zhao_pkg.sv:322-326`). **3,386 is a source audit, not a Quartus result.** The implementation must reproduce it independently through elaboration before treating it as authoritative.

### Two current evidence tools must not be over-read

The current domain scoreboard prints 16,778 fitted ALMs against the 8,000 shell/raster/video/memory allocation, while also identifying five roots without fitted ALMs (`runs/CLAUDE-RUNS/RUN-20260912-1856-ceiling-architecture/domain-scoreboard.txt:6-17` and `:31-38`). It is a sum of selected fitted roots, not a current composed-shell hierarchy report. D3 does not promise to erase the 8,778-ALM difference.

Also, `dsp_census.py` currently maps the shell receipt's `sourceConeParity` field into `clean` (`tools/budget/dsp_census.py:170-182`). Source-list parity is not RTL cleanliness. Consequently, a scoreboard `clean: true` derived through that path is not an independent cleanliness witness. The D3 receipt schema must carry `rtlCleanAtHead` explicitly.

## 2. Smallest safe architecture

```text
zhao_shell_fit_top                         ten real, auto-placed I/O bits
├── u_stimulus     registered GPU-domain protocol models
├── u_shell        zhao_shell_top, unchanged
├── u_gpu_sink     capture + 32-bit sequential MISR + serializer
├── u_vid_sink     capture + 32-bit sequential MISR + serializer
└── u_audio_sink   capture + 32-bit sequential MISR + serializer
```

### Keep `zhao_shell_top` as the core

`zhao_shell_top` is already the real composition. Moving or renaming its roughly 2,000 lines merely to obtain the name `zhao_shell_core` would turn a measurement-instrument change into a large semantic rewrite. It would also disturb manifests, tests, hierarchy names, and historical attribution for no electrical benefit.

The fit wrapper shall instantiate it with the stable instance name `u_shell`. “Core” is its architectural role; a new source-level name is unnecessary.

### Keep the existing simulation composition where it is

The old proposal calls for `zhao_shell_sim_top`, described as the existing wide harness interface (`reports/MHZArchitected:261-272`). The present `tb_zhao_shell` is no longer merely a wiring alias despite its stale header (`tests/shell/tb_zhao_shell.sv:1-7`). It composes mesh fetch, assemble, asset fetch, vertex decode, project, clip, depth quantization, setup, and reciprocal machinery outside the shell; examples appear at `tests/shell/tb_zhao_shell.sv:587-610`, `:751`, `:850`, `:1058`, `:1140`, `:1255`, `:1286`, and `:1353`.

Moving that test composition is unrelated to removing fit virtual pins and carries avoidable simulation risk. D3 must leave it alone.

### Defer `zhao_board_top`

There is no frozen framework, PLL, physical clock, SDRAM-pin, or package-pin integration in this task. A board top written now would encode guesses and invite its characterization result to be quoted as board evidence. Create it only as part of an actual board/framework packet.

### Reject the proposed four fits

The proposal's “core / +binner / +tile pipeline / +full renderer” sequence (`reports/MHZArchitected:279-286`) describes an older composition. The current shell directly instantiates `zhao_geom_bin_pipe` and framebuffer write (`fpga/rtl/common/zhao_shell_top.sv:801-845`); the bin-pipe already instantiates both the binner and tile pipeline (`fpga/rtl/geometry/zhao_geom_bin_pipe.sv:175` and `:240`). Those are not four current subsystem boundaries.

Four expensive fits would therefore either duplicate already-composed logic or require artificial mutant tops. They would violate the repository rule to fit at subsystem boundaries and still would not create a same-RTL comparison to the old receipt. D3 needs one shell-boundary fit.

## 3. Exact fit-top external interface

The generated top shall expose only:

```systemverilog
module zhao_shell_fit_top (
  input  logic       gpu_clk,
  input  logic       vid_clk,
  input  logic       audio_clk,
  input  logic       rst_n,
  output logic [2:0] fit_signature_o,
  output logic [2:0] fit_epoch_o
);
```

Index assignment is frozen:

| index | domain |
|---:|---|
| 0 | `gpu_clk` |
| 1 | `vid_clk` |
| 2 | `audio_clk` |

This is ten real top-level bits: three clocks, reset, three serial signature bits, and three epoch toggles. There is no seed input; each internal generator has a fixed, distinct, nonzero committed seed. There shall be no virtual-pin assignment and no wildcard pin assignment. Physical locations remain auto-selected because this is still not a board pinout.

`fit_epoch_o[d]` toggles when domain `d` snapshots its running signature. On that edge, snapshot bit 0 becomes the post-edge value of `fit_signature_o[d]`; bits 1 through 31 follow on the next 31 rising edges of that domain, least-significant bit first. The epoch bit is a framing/heartbeat witness, not part of the shell signature.

## 4. Mechanical accounting of the shell boundary

The generator shall emit a manifest row for **every declared shell port**, with:

* exact declaration-order ordinal and name;
* direction;
* preserved SystemVerilog type text;
* packed and unpacked dimensions;
* elaborated bit width;
* clock domain;
* driver class for an input or sink class for an output;
* flattened signature offset for an output;
* generated signal and named `.port(signal)` connection;
* dynamic-bit mask or an explicit reason a protocol bit is intentionally constant.

No `.*` connection and no default-domain fallback are allowed. Set equality must hold among parser ports, policy rows, generated connections, and independently elaborated ports. Unknown types, unresolved dimensions, duplicate names, omissions, extra policy entries, and unclassified domains are generation errors.

The current accounting target is:

| shell boundary group | input bits | output bits | input driver | output sink/domain |
|---|---:|---:|---|---|
| clocks and reset | 4 | 0 | real top ports | special, not signed |
| FRAME_RING | 103 | 5 | coherent three-slot model | GPU capture; writes also feed model |
| HPS bridge | 67 | 107 | request-aware HPS responder | GPU capture; requests also feed responder |
| raw pads | 388 | 0 | registered pad producer | — |
| audio | 33 | 80 | GPU ready/valid producer | 14 GPU bits, 66 audio bits |
| displayed pixels | 0 | 40 | — | video capture |
| displayed CRC | 0 | 66 | — | GPU capture |
| frame boundary | 0 | 163 | — | 35 GPU bits, 128 video bits |
| command observability | 0 | 41 | — | GPU capture |
| input observability | 0 | 872 | — | GPU capture |
| counter window | 1 | 83 | bounded registered backpressure | GPU capture |
| memory/status | 0 | 229 | — | GPU capture |
| render + geometry seam | 845 | 205 | frame/triangle/guard models | GPU capture; handshakes also feed models |
| SDR PHY | 16 | 38 | command-aware read-data model | GPU capture; commands also feed model |
| **total** | **1,457** | **1,929** |  |  |

All 1,453 non-clock/reset input bits are GPU-domain shell inputs. Audio samples enter the FIFO on `gpu_clk`; the shell makes that explicit at `fpga/rtl/common/zhao_shell_top.sv:1881-1898`.

Output-domain totals and chunk counts are frozen for the current interface:

| domain | current output bits | 32-bit chunks |
|---|---:|---:|
| GPU | 1,695 | 53 |
| video | 168 | 6 |
| audio | 66 | 3 |

The video set is the nine `px_*`/scaler outputs plus `deadline_faults_o` and `frame_cycles_o`. The audio set is `pcm_valid_o`, `pcm_l_o`, `pcm_r_o`, `underrun_status_o`, and `audio_underruns_o`. Every other current shell output is GPU-domain. These lists must be explicit in policy; they are not naming heuristics.

Flattening is deterministic: shell declaration order, then unpacked elements in declared index order, then packed bits least-significant to most-significant. Packed structs use their SystemVerilog packed layout. The manifest records every offset so an order change is visible in review.

## 5. Registered pseudo-input architecture

A raw LFSR bit is not a protocol model. Every shell input must be driven by a register, but valid/ready, request/response, and frame transactions must also remain legal enough to keep useful cones active.

### Common rules

* A 64-bit maximal-length generator with a fixed nonzero domain seed supplies changing payload entropy. Payload expansion is registered; no shell data input is driven directly from a combinational LFSR slice.
* A payload controlled by `valid` remains stable until acceptance.
* Ready/backpressure sources may vary but have a committed maximum stall, so deterministic traffic cannot deadlock.
* ROM entries provide known-valid command and triangle transactions. LFSR traffic varies otherwise-unused payload bits and supplies occasional deliberate negative transactions; it does not randomly assert protocol control bits.
* Driver decisions use the registered shell-output capture, not a long combinational feedback path from `u_shell`.
* No traffic begins until the GPU wrapper reset release has completed. SDRAM/render traffic additionally waits for `init_done_o` where required.

### Driver classes

**FRAME_RING.** Maintain three registered slot records. Drive the legal `FREE -> ARM_WRITING -> READY` sequence with a sealed byte length. Apply accepted shell writes (`DONE` or `FREE`) from the captured ring-write channel, wait a bounded host delay, then recycle the slot. `ring_wr_ready_i` is registered and may stall for a bounded number of cycles. The command bytes corresponding to a READY slot come from a small committed ROM derived from an existing directed shell packet, not random words.

**HPS responder.** Detect a captured `hps_req_valid_o`, wait a deterministic 1–4 GPU cycles, and pulse registered `hps_req_grant_i`. For reads, wait the existing 16-cycle first-beat profile, then emit one registered 64-bit beat per cycle for `ceil(len/8)` beats with `hps_rd_last_i` only on the final beat. The bridge contract permits 1–64 bytes and requires 64-byte-aligned requests (`fpga/rtl/memory/zhao_hps_bridge.sv:11-24`). Command-range reads select the packet ROM; other reads select changing registered data. For writes, consume and account for captured `hps_wr_valid_o` through `hps_wr_last_o`. A timeout is a simulation failure.

**Pads.** Update the six pad input ports from registered state at a slow deterministic cadence, with all four controllers alternately present. Directed values cover signed-axis extrema, center, and button changes; LFSR phases exercise remaining bits.

**Audio producer.** Hold `aud_wr_valid_i`, left, and right samples stable until captured `aud_wr_ready_o`; then advance a deterministic stereo sequence. Include bounded pauses so both normal consumption and underrun status become active.

**Counter consumer.** Drive `cnt_snap_ready_i` with registered bounded backpressure. It must accept complete windows often enough for all provider entries to be observed.

**Render producer.** Use a frame FSM: select the framebuffer writer, pulse begin, submit a small ROM of coherent, in-range triangles while holding all fields through `render_tri_ready_o`, pulse end, and wait for drain/retirement before recycling. Alternate valid directed jobs with width-covering payload jobs so synthesis does not see constant high fields. Any area experiment must retain the valid ROM lane; random coefficients alone are not evidence that raster work occurred.

**Geometry guard client.** Hold a registered request until `geom_guard_rsp_o.ready`. Generate aligned, bounded legal reads in the geometry asset window, one outstanding request at a time, and occasionally issue a deliberate denied request so the guard/refusal cone fires. The client, length, address, and byte-enable fields remain mutually coherent for the legal lane.

**SDR PHY responder.** Decode captured SDR commands. On a read command, schedule registered `phy_dq_i` data at the controller's configured read latency; on writes, observe `phy_dq_o`, output-enable, mask, bank, and address. It is a compact command-aware responder, not an inferred full SDRAM array and not random data presented every cycle.

This traffic is “legal-ish” because it obeys each boundary handshake and provides positive valid transactions. It is not a functional board/HPS/SDRAM model, and the receipt must say so.

## 6. Domain-local output consumption and signatures

### Boundary capture first

Every one of the 1,929 shell output bits is captured in a register on its producer clock before any selection or reduction:

* 1,695 GPU capture bits;
* 168 video capture bits;
* 66 audio capture bits.

Capture registers need not carry reset; signature logic ignores them until one local warm-up interval has elapsed. They become defined after the first native-domain edge while the shell is reset. Avoiding reset on this wide bank prevents an artificial global reset tree. The capture is the immediate shell endpoint, so the large chunk selector cannot lengthen a shell output path.

These registers are intentional wrapper cost. They are not to be counted as `u_shell` logic.

### Sequential 32-bit MISR

Each domain has one independent sink:

1. Select one static 32-bit chunk from the domain capture bank using a generated `case` on a registered chunk index. Zero-pad only the final chunk.
2. Register that selected chunk.
3. On the following local edge, update a 32-bit MISR with the selected word.
4. Advance modulo the domain's manifest chunk count.
5. At a complete sweep, if the serializer is idle, copy the running MISR to a 32-bit snapshot, toggle `fit_epoch_o[d]`, and serialize the snapshot least-significant bit first. The running MISR continues; short-domain sweeps may be skipped while serialization is busy.

Use the fixed recurrence

```text
next = {misr[30:0], 1'b0}
       XOR (misr[31] ? 32'h0040_0007 : 32'h0)
       XOR selected_chunk
```

Initialize the MISR to zero and do **not** inject the chunk index, epoch, heartbeat, or a nonzero signature seed. Otherwise a lively wrapper could manufacture a nonzero signature from an all-zero shell. The heartbeat is deliberately separate.

The selector and MISR are downstream of capture registers and therefore wrapper-only timing. There is no giant XOR tree and no cross-domain reduction.

### CDC and reset

No wrapper payload crosses clock domains. Each output is captured, folded, snapshotted, and serialized in its own producer domain. The three bits of each top-level output vector are unrelated physical outputs; no logic combines them.

The shell continues to receive the external `rst_n` exactly as it does now. The wrapper shall not silently change the shell specimen by substituting a GPU-synchronized reset for all three domains. Wrapper-local state uses asynchronous assertion and a two-flop, domain-local synchronous release, followed by at least eight local warm-up edges. No false path, multicycle path, or invented reset exception is added.

Keep the existing clock intent: 10 ns GPU, 20 ns video, and 40 ns audio (`fpga/quartus/shell_fit/zhao_shell_fit.sdc:1-6`); audio remains asynchronous to GPU/video, while GPU/video remain related (`fpga/quartus/shell_fit/zhao_shell_fit.sdc:8-17`). D3 adds no new inter-domain path.

## 7. Pruning and vacuity defenses

Source connectivity alone is necessary and insufficient. D3 needs all four layers below.

### 7.1 Generation proof

* Exact set equality across shell declaration, policy, generated signals, named instance connections, and signature offsets.
* Independent elaboration census reproduces 154 names and 3,386 bits for the current source.
* Every non-clock input has exactly one registered driver and every output has exactly one domain capture; protocol feedback is additional fanout, not a substitute for signature consumption.
* Generated boundary aliases receive Quartus-supported keep semantics; source and capture registers receive preserve semantics. These attributes live only in generated wrapper RTL, never in `zhao_shell_top`.
* The implementation must verify that Quartus 17 honors the chosen attributes. An ignored assignment is a failed instrument, not a warning to waive.

### 7.2 Simulation activity proof

A deterministic three-clock smoke test must demonstrate, after reset:

* all three LFSR/driver clocks advance;
* each non-clock input port changes or is listed with an intentional constant-bit mask;
* at least one ring transaction, HPS read completion, audio acceptance, render acceptance and drain, geometry guard accept and reject, SDR read response, and counter-window acceptance occurs;
* every output capture group changes at least once where the directed transaction is expected to affect it;
* all three chunk indices wrap, epochs toggle, and serialized signatures differ from an all-zero-shell reference.

The existing shell directed tests remain the functional authorities. This smoke test proves only that the fit harness is non-vacuous.

### 7.3 Post-map structural proof

A manifest-driven post-map witness must find every generated input register and output capture bit in the synthesized netlist, with a path toward or from `u_shell`, and find each domain sink feeding its real top-level pins. It must also find a nonzero `zhao_shell_top:u_shell` entity row with registers, logic, RAM, and DSP resources. Missing or renamed nodes fail loudly; do not reduce this to an “at least 50 ports” threshold.

### 7.4 Fitter proof

The fitter summary must report:

* zero virtual pins;
* ten real pins, unless Quartus documents a different counting rule in the raw report;
* a successful fit on the provisional device;
* all three clocks present;
* no “clock port is fed by virtual pin” warning;
* the `u_shell` fitted hierarchy row and separate wrapper rows.

Zero virtual pins is itself a zero-reading detector and therefore needs a fire control.

## 8. Generator and test design

### Side-effect-free parser/generator

Do not make the PowerShell parser authoritative. It strips built-in keywords and then accepts every remaining identifier (`tools/quartus/run_shell_fit.ps1:142-180`), so `input var zhao_guard_req_t geom_guard_req_i` makes it treat the typedef name as a port. Its minimum-count check catches an empty parser, not a plausible undercount (`tools/quartus/run_shell_fit.ps1:183-199`).

Also do not import `gen_prod_top.py` wholesale. It contains useful repaired handling for comma continuations and user-defined types (`tools/quartus/gen_prod_top.py:83-170`), but its argument handling recognizes only the presence of `--check`; any other argument, including `--help`, falls through to writing the output (`tools/quartus/gen_prod_top.py:562-591`).

The implementation packet should add:

* `tools/quartus/shell_ports.py`: side-effect-free declaration model and policy validation;
* `tools/quartus/gen_shell_fit_top.py`: explicit `argparse` commands, with `--check` read-only and `--write` atomic;
* `design/shell_fit_ports.yml`: exact per-port driver/domain policy;
* `fpga/rtl/generated/zhao_shell_fit_top.sv`: committed generated wrapper/helpers;
* `fpga/rtl/generated/zhao_shell_fit_top.manifest.json`: committed machine-readable accounting and source hashes.

The lexer may be based on the repaired production parser, but a second frontend must validate its result. Use the repository's normal SystemVerilog elaborator to compare port names, directions, shapes, and `$bits`; do not ask two copies of the same regex whether each other is correct.

Generated headers must include hashes of the shell declaration, policy, and generator. `--check` compares bytes and never writes. `--help`, malformed arguments, and imports never write.

### Tests and deliberate fire controls

Add ordinary pre-fit tests for:

1. parser fixtures covering header imports, parameters, `var`, signed packed vectors, multiple names per declaration, unpacked arrays, and packed typedef ports;
2. exact generation freshness;
3. current shell/policy/manifest set and bit-count equality;
4. lint/elaboration of the generated top with the same source pool as the fit;
5. deterministic three-clock traffic and signature activity;
6. QSF top/source parity and an assertion that no `VIRTUAL_PIN ON` remains;
7. receipt parsing and fitted-hierarchy parsing from committed text fixtures.

Every detector gets a positive control:

* Delete one policy row in-memory: generation must name the omitted shell port.
* Add a fake typedef token: the parser must not report it as a port.
* Disconnect one output capture in-memory: structural accounting must name that output.
* Freeze each sink in turn: the activity test must identify the dead domain.
* Feed the receipt parser a summary with `Total virtual pins : 1`: the zero-virtual-pin gate must reject it.
* Feed the hierarchy parser a report lacking `u_shell`: attribution must reject it.
* Change one generated byte: `--check` must report stale output without rewriting it.

One committed mutant fixture is warranted: `tests/tools/fixtures/shell_fit_missing_sink.json`, derived from a small complex-port fixture and deliberately omitting its second output sink. A unit test must assert the exact omission diagnostic. It is a test fixture, not synthesizable production RTL. Do not commit a stale or disconnected version of the real fit wrapper.

## 9. Fit project, provenance, and attribution

### Source pool

For the first D3 change, keep the existing ordered `ZHAO_SHELL_RTL` source pool and add the generated wrapper source explicitly. The QSF currently mirrors that pool (`fpga/quartus/shell_fit/zhao_shell_fit.qsf:111-193`), including some D22 modules used only by the simulation bench (`:166-192`). Uninstantiated source files do not alter the fitted hierarchy; splitting that broad CMake pool is separate cleanup and should not be bundled with the measurement change.

Replace “virtual-pin parity” with these preflight checks:

* QSF top is exactly `zhao_shell_fit_top`;
* QSF source pool equals the designated CMake pool plus the generated wrapper/helpers, in defined order;
* generated artifacts are current at committed HEAD;
* no virtual-pin assignment exists;
* the manifest and independent elaboration census agree.

Call the first boolean `compileSourcePoolParity`, not `sourceConeParity`: a compile pool containing uninstantiated files is not an elaborated cone.

### Clean provenance

Continue fitting a temporary `git archive HEAD`; that is a good property of the existing runner (`tools/quartus/run_shell_fit.ps1:221-250`). Before archiving, compare every source/policy/generator/constraint file in the measurement packet against HEAD and refuse if that measurement cone is dirty or an expected generated file is untracked. Unrelated dirty run notes need not poison the specimen.

Receipt schema version 2 must record, separately:

* `sourceCommit`;
* `rtlCleanAtHead`;
* `compileSourcePoolParity`;
* generated file and manifest hashes;
* shell declaration/policy/generator hashes;
* tool and device;
* stage status;
* total resources and virtual/real pins;
* per-entity map and fitted resources;
* wrapper traffic/profile version;
* limitations.

Update `dsp_census.py` to consume `rtlCleanAtHead`, never `compileSourcePoolParity`, when this schema becomes authoritative.

### Separate wrapper cost from shell cost

The fit summary's integer ALM total is **top + wrapper + shell**. It must never be published as shell-only cost.

Quartus already emits a “Fitter Resource Utilization by Entity” table with `ALMs needed`, final-placement ALMs, dense-packing recovery, unavailable ALMs, registers, RAM, DSP, pins, and virtual pins; an existing raw example shows the fields at `reports/synthesis/blockpaths/zhao_texture_island_top.fit.rpt:3821-3825`. The runner must harvest:

* the total `zhao_shell_fit_top` row;
* the total `zhao_shell_top:u_shell` row, including descendants;
* top-self and each `u_stimulus`/sink row;
* any unaccounted remainder rather than assigning it by subtraction.

Use the `u_shell` row's **`ALMs needed` total** as the canonical fitted shell attribution, retaining its fractional value and all supporting columns. Also preserve the map report's entity ALUT/register figures. The existing entity census explains why total and self must be distinguished (`tools/quartus/entity_census.py:4-10`) and why a remainder may not be silently absorbed (`tools/quartus/entity_census.py:139-156`).

Do not calculate `wrapper ALMs = fit-summary ALMs - u_shell ALMs`. Dense packing can share an ALM across hierarchy boundaries, and the fitter's own fractional attribution exists precisely because simple subtraction is not exact.

Do not add a design partition merely to force cleaner arithmetic. A partition changes optimization and packing and therefore changes the specimen. If Quartus drops the `u_shell` row despite the stable instance hierarchy, D3's attribution check fails; a partitioned paired characterization then requires a separate proposal. It is not an automatic second fit.

### Timing meaning

The immediate registered inputs and output capture banks make shell-boundary paths register-to-shell or shell-to-register. The chunk mux, MISR, snapshot, and serializer are downstream of capture and cannot lengthen a shell path.

TimeQuest output must classify, rather than mix:

* shell-internal paths, both endpoints under `u_shell`;
* stimulus-to-shell boundary paths;
* shell-to-capture boundary paths;
* wrapper-only signature paths;
* the shell's existing cross-domain paths.

Timing may be red and D3 may still be a valid characterization. No I/O delay, PLL, package, final clock, or reset exception is invented to make it green.

## 10. The one subsystem-boundary fit gate

**Gate name:** `shell_fit_top_clean_characterization`

**Exact question:**

> On clean committed HEAD, does the generated `zhao_shell_fit_top` complete analysis, map, fit, and TimeQuest on provisional `5CSEBA6U23I7` with its exact ten-bit real interface, zero virtual pins, complete manifest and post-map port witnesses, all three constrained clock domains, and a separately reported nonzero `zhao_shell_top:u_shell` fitted hierarchy row?

That is the only D3 fit gate. Timing pass/fail at 10/20/40 ns is a recorded result, not silently made an acceptance condition for the measurement instrument. Parser, lint, generation, traffic, mutation, and report-fixture tests are prerequisites, not additional fit gates.

## 11. What the result can and cannot mean

A passing D3 fit can say:

* current committed `zhao_shell_top` elaborates and fits on the provisional device under registered synthetic boundaries;
* its fitted hierarchy cost is attributable separately from the measured wrapper rows;
* shell-internal and shell-boundary timing under the stated 10/20/40 ns assumptions is known;
* virtual-pin clock and packing contamination is absent from that specimen.

It cannot say:

* how many ALMs D3 “saved” versus the old fit—the RTL and boundary are different;
* that subtracting the historical 1,608 gives the new answer;
* that a final board, framework, PLL, SDRAM PHY, or pinout fits;
* that the console at game capacities fits;
* that uncomposed production roots cost zero;
* that a synthetic responder is equivalent to HPS or SDRAM hardware;
* that one seed is a placement-noise distribution.

For a later shell optimization with unchanged ports, use the same generated-wrapper hash, traffic profile, device, settings, and seed before/after, and compare the `u_shell` hierarchy row plus shell-internal timing. If ports change, disclose the wrapper delta and do not use top-total subtraction as attribution.

## 12. First post-D3 ALM-lowering target

The first non-terrain, non-projection resource experiment should target **`zhao_raster_edgewalk.g_col`, the replicated 16-column row evaluator** (`fpga/rtl/raster/zhao_raster_edgewalk.sv:373-451`).

Why this target, stated with the evidence limits intact:

* The only available standalone fit lists edgewalk at 2,286 ALMs, larger than the nearby historical binner and early-Z rows, but that receipt is explicitly dirty and carries 220 virtual pins (`reports/synthesis/zhao_block_fit.json:762-785`). It is candidate evidence, not a current cost.
* Current RTL still spatially replicates three edge-offset constructions and three fill decisions across all 16 columns. It also records an approximately 138-ALM register trade made for timing (`fpga/rtl/raster/zhao_raster_edgewalk.sv:218-248`). This makes the current area mechanism visible even though its current fitted size is unknown.
* D3 will provide the first current composed hierarchy row for `u_shell|u_render_bin|u_pipe|...edgewalk`, allowing the dirty standalone ranking to be confirmed or refuted without virtual-pin attribution.

The experiment's hard constraints are: preserve exact top-left fill behavior, 16 pixels per row, one row per walk clock, the 16-cycle walk, and the existing two-DSP setup architecture. A proposal that merely serializes columns to make the ALM number smaller is not acceptable. First inspect the D3 hierarchy and packing columns; then test an area architecture for the offset network under the identical wrapper. If the D3 row shows edgewalk is not a leading current owner, that measurement vetoes the experiment before RTL is touched. That is D3 enabling a real decision rather than blessing a stale ranking.

The shell observability trees are the next low-risk candidate, not the first assumed winner. The old brief called them a three-way guard sum and two five-client totals (`reports/MHZArchitected:288-296`); current RTL has grown to a four-way 32-bit guard sum (`fpga/rtl/common/zhao_shell_top.sv:721-727`) and two seven-entry 64-bit totals (`fpga/rtl/common/zhao_shell_top.sv:1965-1984`). They are real avoidable cones, but there is no current fitted evidence that they dominate area, and their exact snapshot semantics must be preserved.

D3 itself claims **zero ALM reduction**. Its contribution is the clean baseline and hierarchy attribution needed to choose and verify the first reduction.

## 13. Not verified in this architecture pass

No Quartus process was launched. Therefore none of the following is verified yet:

* that the proposed wrapper compiles in Quartus 17;
* that the current declaration census is reproduced by an independent elaborator;
* that auto-placement yields exactly ten reported real pins;
* that the keep/preserve spelling is accepted and honored by Quartus 17;
* that every directed pseudo-transaction reaches its intended completion;
* that all three signatures become traffic-sensitive and non-vacuous;
* that the current shell fits or meets any clock through this wrapper;
* that the current fitter retains the required `u_shell` hierarchy row;
* the wrapper's ALM/register cost;
* the current composed ALM ranking, including edgewalk;
* any board, framework, PLL, SDRAM, or hardware behavior.

The historical 3,214/1,608 artifact is verified only for its archived historical specimen. It is not current evidence.

## 14. Implementation-packet boundaries

### Packet A — instrument generation, no fit

Add the parser library, exact policy, generated top/manifest, complex-port and missing-sink fixtures, lint/elaboration tests, traffic smoke test, source-pool preflight, and all fire controls. Leave `zhao_shell_top`, `tb_zhao_shell`, production logic, and board logic unchanged.

### Packet B — dedicated fit flow and one characterization

After Packet A is committed and clean, change the QSF top, remove virtual pins, update the runner/receipt/hierarchy parsers, run the single `shell_fit_top_clean_characterization` gate from `git archive HEAD`, and archive raw map/fitter/TimeQuest evidence. Do not combine this with an ALM optimization.

### Packet C — resource experiment

Only after D3 attribution is reviewed, open a separate edgewalk area packet. Preserve the throughput/fill constraints above, run existing functional/formal tests, and compare an unchanged-wrapper baseline and candidate under identical settings. If D3 refutes edgewalk's ranking, stop and select the largest measured eligible shell/raster owner instead.

A simulation-wrapper rename, source-pool cleanup, partition experiment, and future board top are separate packets. None is required to close D3.
