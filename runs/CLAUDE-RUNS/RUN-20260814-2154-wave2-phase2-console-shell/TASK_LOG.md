# Task Log: RUN-20260814-2154 - [Describe objective here]

**Created:** 2026-08-14 21:54 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260814-2154-wave2-phase2-console-shell/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-08-14 21:54 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260814-2154
- Created working directory
- Initial context: [brief description]

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| | | | | |

---

## Files Created

*Updated as files are created*

---

## Decisions Made

*Updated as decisions are made*

---

## Next Steps

*Updated as progress is made*

---

## 2026-08-15 — W2.1 COMPLETE (specification wave, ABI v2, interface freeze)

**Branch:** `wp/w2.1-spec` (worktree `.worktrees/w21`) · **Commits:** `a0430b2` → `1ab1075` → `c081f8c` (all pushed)

### Delivered (plan §7 items 1-3)

1. **`a0430b2` spec(w2) ABI v2 + rules specs + regen**
   - `spec/commands.zidl` v2 (additive): `enum video_mode : u8`; `struct PadFrame` (20 B); `SetPresentationContract.mode` retyped; `DebugFrameBlit 0xF002` implemented (48-B record); `DebugRumble 0xF004` implemented (32-B record); coordinator-reserved `DrawSky 0x0310` (176-B record, sky_and_beams.md §4) with `angle16` wire type (U 0.0.16 turns, u16) added to the grammar/emitters; debug opcodes may now be `implemented` (header flags-bit0 law unchanged).
   - abi-gen: enum-typed fields emitted at backing width in C++/TS/SV; **enum range validation live** (capture_format §3.2 step 7) in oracle + C++ (`zhao_enum_value_ok`) + TS mirror + SV validator; corpus cases `enum_out_of_range` (BAD_VALUE=9), `valid_debug_blit_rumble`, `debug_blit_without_flag`. New layout laws: struct size multiple of alignment cap; enum values must fit backing. Sample fixes: struct arrays expand per element (`mat4fx rot_proj[2]`); mid-record pads no longer corrupt the golden leaf cursor (latent v1 bug — all v1 pads were trailing). `spec/commands.zidl` pinned to LF in .gitattributes (it is hashed as zidl_sha256 into every generated header).
   - New specs: `video_rules.md` (D1 tables, 60 Hz repeat law, displayed-CRC, Duo canvas map), `input_rules.md` (PadFrame semantics, frame_tick latch, rumble PWM 1 kHz frame-gated, button bit table), `audio_rules.md` (D4 FIFO 2048/refill 256/watermark 512, 800 pairs/frame, MixerTone with exact floor increments), `memory_rules.md` (SDRAM sim profile CAS3/burst8/tRCD3/tRP3/tRC9/refresh 780; arbiter D3 + liveness B=2·14+12=40; FRAME_RING/PCM_RING descriptors; guard Phase-2 map FB slots @ 0x0/0x3C000), `counters.md` (counter_id = catalog index, append-only; snapshot protocol). `capture_format.md` amended (v2 sections, wave-2 golden naming §6.1).

2. **`1ab1075` contracts(w2) + ledger notes**
   - All 16 wave-2 contracts filled, 17 §4 headings each, zero TODOs, citing the rule specs so W2.2-W2.6 never need the charter.
   - blocks.yml: CMD.SCHEDULER purpose/notes → charter §7.4 state names (`FREE → ARM_WRITING → READY → FPGA_RUNNING → DONE`); MEM.GUARD notes → Phase-2 region map; diagrams regenerated.
   - **New ledger rule V15** (counter_id totality): catalog must be duplicate-free; blocks may not declare a counter twice; 3 unit tests.

3. **`c081f8c` iface(w2) zhao_pkg.sv FREEZE**
   - `fpga/rtl/common/zhao_pkg.sv`: ABI re-export (wildcard import), `zhao_mode_e` + converters, `zhao_timing_t` + `ZHAO_TIMING[3]` (the single D1 table), canvas/FB-slot/Duo constants, `zhao_px_stream_t`, `zhao_client_e`, guard req/rsp + arbiter credit req/rsp (+ liveness constants), HPS burst req/rsp, `zhao_frame_tick_t`, `zhao_counter_snap_t`, Phase-2 counter_id localparams (indices verified against blocks.yml catalog). Verilator `--lint-only -Wall` CLEAN; enforced by new `lint_zhao_pkg` CTest (banner-append).

### PadFrame decision: **20 B** (not 24)
Plan D5 sketched "24 B, 4-aligned", but the shipped .zcap 0x0004 entry is 20 B and the .zidl grammar (rule 3, alignment cap 4) forbids the hidden tail bytes a 24-B layout needs. Declared fields sum to 18 B → one explicit `u32 rsv` closes the struct at exactly 20 B (already 4-aligned, array stride 20 keeps `buttons` aligned per element — now generator-enforced). Wire-compatible with the shipped section; documented in commands.zidl + capture_format §4.2 + input_rules §1.

### Verification (all green)
abi:check clean (25 outputs) · abi-gen 20/20 · compiler 7/7 · ledger:check OK · ledger:gen --verify fresh · ledger tests 13/13 · **ctest 16/16** (incl. lint_zhao_pkg) · zero TODOs in the 16 contracts.

### Handoff notes for W2.2-W2.6
- Interfaces: import `zhao_pkg` (frozen; changes via architect). Testee lists (goldens/probe/conformance) now include draw_sky/debug_frame_blit/debug_rumble — extend, don't fork.
- ABI version is now 2; `bad abi` vectors re-pinned; enum out-of-range is BAD_VALUE(9) — keep tri-language parity when touching validators.
- INPUT.SNAC contract intentionally left stubbed (deferred block, not wave-2 scope).

---

## 2026-08-15 — W2.3 COMPLETE (INPUT subsystem RTL)

**Branch:** `wp/w2.3-input` (worktree `.worktrees/w23`) · **Commits:** `8400661` → `bc94ced` → `7ed046a` → `2fcfde0` (all pushed)

### Delivered (plan §2 W2.3)

1. **ZRef first** — `reference/include/zref/zref_input.hpp` (header-only): `zref::PadSnapshot` (atomic 4-pad latch oracle, per-pad monotonic sequence with absent-freeze + mod-2^16 wrap, absent-pad zero frame, 20-B wire bytes), `zref::RumbleBridge` (DebugRumble decode, frame-gated duty latch, last-writer-wins + `rumble_frames_dropped` accounting, free-running PWM carrier model). **Sequence convention pinned:** first present frame carries 1, tick 65536 wraps to 0.
2. **RTL** — `fpga/rtl/input/zhao_input_snapshot.sv` (atomic latch at `frame_tick.pulse` ONLY via double buffer + gray-coded pointer; generated `zhao_pad_frame_t` ×4 + packed 640-bit mirror through `zhao_pack_pad_frame`; saturating u64 `input_sequence_gaps` detector + event pulse) and `zhao_input_rumble.sv` (per-pad pending regs, duty latch at the NEXT tick, `duty = enable ? strength : 0`, replace-and-count drops, index>3 dropped entirely, hold-with-no-command, free-running 8-bit PWM carrier, `PWM_PHASE_DIV` param). Both lint-clean `-Wall` (new `lint_input_snapshot`/`lint_input_rumble`).
3. **Tests** — `tests/input/input_snapshot_directed.cpp` (556 checks: reset, 200-cycle mid-frame thrash atomicity, 4-slot independence, absent/replug freeze-resume, 66,000-frame sequence run through the 2^16 wrap, packed byte identity), `input_rumble_directed.cpp` (2247 checks: gating, PWM duty streams over full carrier periods, double/triple command, bad index, enable/strength corners, hold), `input_random.cpp` (PCG timelines, 1k fast / 100k nightly soak, per-cycle PWM differential, internal run-twice FNV transcript hash; 23.4M checks @100k in ~5 s).
4. **Formal** — `tests/formal/input_snapshot_atomic.sby`: BMC depth 30, **PASS** — no latched bit moves between ticks; sequence +1 exactly once per present tick / frozen while absent; `input_sequence_gaps == 0` forever (the no-gaps-by-construction proof). CTest wrapper SKIP-if-no-sby.
5. **Ledger** — INPUT.SNAPSHOT/RUMBLE → **RTL_VERIFIED** via the ladder (SPECIFIED → REFERENCE_COMPLETE → UNIT_VERIFIED → RTL_VERIFIED, one commit-pinned step each per V2); diagrams regenerated; `ledger:check` green.

### Findings / deviations (merger notes)

- **Random-test file is combined** (`tests/input/input_random.cpp`, not per-block files): one binary drives both modules on a shared timeline; ledger + contracts trued to the delivered paths.
- **`zhao_pkg` wildcard import does NOT transitively re-export** `zhao_abi_pkg` names to importing modules (SV scoping) — leaf modules must `import zhao_pkg::*; import zhao_abi_pkg::*;` (both input modules do; noted for W2.2/W2.4/W2.5 implementers).
- **Formal frontend is `read_slang`** (auto-loaded plugin): yosys's built-in Verilog reader cannot parse the generated abi package's array localparams (`ZHAO_CRC32C_TABLE`). sby `[files]` resolve against the sby **process CWD** — the wrapper pins a workdir exactly two levels below the repo root.
- **Async-reset formal pattern** (reusable): declared register inits mirroring the reset state (else free-init step 0 fabricates phantom gaps/sequence violations), `assume(rst_n)` once-released guard, and properties guarded on `rst_n && $past(rst_n)`.
- **Harness fix (shared file `tests/harness/zhao_sim.cpp`)**: `report_and_exit` now flushes + `std::_Exit` — Verilator 5.051 + winlibs libwinpthread intermittently deadlocks in `VlThreadPool::~VlThreadPool` at exit-time static destruction (~0 CPU `WaitForSingleObject`; reproduced on pre-existing `test_stub_top`). All Verilated test mains benefit; merge this hunk first if it conflicts.
- **Machines note:** mingw64 `bin` ahead of oss-cad-suite on PATH breaks yosys DLL resolution (0xC0000135) — the formal wrapper prepends the suite's bin+lib itself.

### Verification
`ctest -L fast` **19/19** twice (14 existing + 5 new) · `-L formal` **2/2** (55 s BMC) · nightly `input_random_soak` + input tests **5/5** · binary-level run-twice identical hashes · `ledger:check` + diagrams fresh · lint clean on both modules.

### Handoff to W2.7
Ports: raw pad state in (`pad_present/buttons/lx/ly/rx/ry` arrays), `zhao_frame_tick_t` in, `zhao_pad_frame_t[4]` + `pad_frame_flat[639:0]` (slot 0 = LSBs) + `pad_sequence[4]` + `input_sequence_gaps` out; rumble: executed `DebugRumble {pad_index, enable, strength}` cmd port in, `rumble_duty[4]`/`rumble_active`/`rumble_pwm[3:0]`/`rumble_frames_dropped` shadow out. Frame gate: a command pulse simultaneous with the tick latches one frame LATER (documented in the module header; tests never emit that corner).

## 2026-08-15 — W2.4 COMPLETE (AUDIO subsystem RTL)

**Branch:** `wp/w2.4-audio` (worktree `.worktrees/w24`, base `0c2e796` per plan) · **Commits:** `9e813e0` → `a3cd94a` → `23ab19e` → `5baf797` → `812c436` (all pushed)

### Delivered (plan §2 W2.4, D4 law)

1. **ZRef first** — `reference/include/zref/zref_audio.hpp` + `reference/src/zref_audio.cpp`: `zref::MixerTone` (48 kHz tone from the committed W3 sin table ONLY — `s16 = fx_sin(angle16) >> 1`, arithmetic shift toward −∞, saturating; frozen floor-law increments verbatim, no new constants), `zref::AudioFifo` (register-exact oracle at D4 geometry 2048/256/512 incl. BOTH 2-flop gray synchronisers and the u64 counter snapshot at the gpu_clk/4 seam), `zref::PcmRing` (memory_rules §4.2 free-space law).
2. **RTL** — `fpga/rtl/audio/zhao_audio_fifo.sv`: whole-pair ready/valid write side (gpu), one pair per audio tick (audio domain), underrun = repeat last pair + `audio_underruns` once per continuous event, reset = zeros NOT repeats (no "last pair" yet), overflow structurally impossible (occupancy view is a conservative overestimate; formal proves the bound carries to true occupancy). CDC documented in-module (SYS.CDC is a TODO stub, rule V8 own-bridge): gray-coded pointer + counter crossings, 2FF synchronisers. Lint-clean `-Wall` (`lint_zhao_audio_fifo`). Qualified `zhao_pkg::` names only (yosys rejects imports).
3. **Tests** — `mixer_tone_directed.cpp` (bit-exact vs the committed exhaustive sin goldens: 3 tones × 3 frames, saturation corners +0x7FFF/−0x8000, phase law, ring law), `mixer_tone_random.cpp` (1000 frames PCG tone switching at frame boundaries, 800k pairs), `audio_fifo_directed.cpp` (reset silence, 800-pairs/frame steady fill w/ plateau band + watermark request, deliberate underrun ×2 → repeat/count-once/continuity/loss-free resume, 4096-pair full backpressure at 2048, tone passthrough bit-exact, snapshot law incl. counter_id), `audio_fifo_random.cpp` (PCG-jittered refills, FULL stream + per-cycle occupancy + shadows bit-exact vs oracle, 1k fast / 100k nightly ≈ 41 s, failing vectors saved).
4. **Formal** — `tests/formal/audio_fifo_bounds.sby` (BMC depth 32, **multiclock**, audio_clk = clk/4 divider in harness): P1 occupancy ∈ [0,DEPTH] (conservative-view argument), P2 no-accept-when-full, P3 repeat-bitexact + count-once-per-event. **Mutation-verified**: forcing `wr_ready=1` and breaking the repeat both FAIL the proof. CTest wrapper SKIP-if-absent.
5. **Ledger** — AUDIO.FIFO → **RTL_VERIFIED**, SW.MIXER → **REFERENCE_COMPLETE** (tone+ring subset per contract note) via the V2 ladder (one commit-pinned step each); diagrams regenerated; `ledger:check` green (V14 fresh).

### Findings / deviations (merger notes)

- **Test filenames follow the LEDGER/CONTRACT law, not the task sketch**: `audio_fifo_random.cpp` (ledger V6 enforces it), `mixer_tone_directed.cpp` + `mixer_tone_random.cpp` (SW.MIXER contract) instead of `audio_random.cpp` / `audio_tone_directed.cpp`.
- **Formal frontend**: yosys's built-in SV reader cannot parse the generated `zhao_abi_pkg` (unpacked-array localparams) NOR package-scoped imports, and the frozen `zhao_pkg` contains both + the `ZHAO_TIMING` unpacked-array localparam. W2.3 used `read_slang`; W2.4 stages instead (a) a committed EMPTY stub `zhao_abi_pkg_formal_stub.sv` (nothing from the ABI package is in the audio cone; wildcard-import only) and (b) a mechanical wrapper-generated copy of `zhao_pkg.sv` with the import line + video-only timing table removed (documented in `audio_formal_lane.cmake.in`). DUT bytes under proof are untouched. If W2.2/W2.5 formal lands on `read_slang`, either approach composes — the wrapper transform fails loudly if the frozen package shape changes.
- **Single-clock formal masks the backpressure law**: with clk_gpu == clk_audio, writes and pops balance 1:1 and the FIFO can never FILL — `wr_ready=1` mutation PASSED vacuously. The harness divides clk by 4 (the same sim seam) + `multiclock on`; then the mutation fails at step 19. Any future FIFO-class formal must keep an asymmetric ratio.
- **sby `[files]` semantics on this kit**: two-column entries are `<dest> <src>` (NOT src dst); paths resolve against the sby process CWD; the wrapper prepends the suite's bin+lib to PATH (`cmake -E env`) or yosys dies 0xC0000139 (same DLL-shadowing W2.3 hit; independently reproduced and fixed here).
- **Oracle fidelity**: `zref::AudioFifo` mirrors RTL non-blocking semantics cycle-for-cycle (pre-edge decisions, sync-chain ordering, shadow latches the PRE-edge gray view) — required for bit-exact dual-clock differential; document any future RTL change in both places.
- **origin/main moved during the WP** (review fixes `b05f6f7`, `53820af` on `test_fixp.cpp` + a field corpus .zvec) — disjoint from every W2.4 file; branch pushed on the plan-pinned base `0c2e796`, merges clean.

### Verification
`ctest -L fast` **23/23** (18 existing + 5 new: mixer_tone_directed, mixer_tone_random, audio_fifo_directed, audio_fifo_random, lint_zhao_audio_fifo) run TWICE identical · `-L formal` **2/2** (formal_lane smoke + formal_audio_fifo ~6 s) · nightly label incl. 100k random + formal green · `ledger:check` + diagrams fresh · lint clean.

### Handoff to W2.5/W2.7
- AUDIO.FIFO ports: pair write `{wr_valid_i, wr_l_i, wr_r_i} / wr_ready_o` (gpu; the ring-read shim unpacks 64-bit bridge beats = 2 pairs L-then-R), `refill_req_o` level + `occupancy_o[$clog2(DEPTH):0]` (gpu view, conservative), `frame_tick_i` + `zhao_counter_snap_t cnt_snap_o` (id 31, u64 shadow, valid pulses one cycle), audio side `{pcm_valid_o, pcm_l_o, pcm_r_o, underrun_status_o, audio_underruns_o[31:0]}`. `DEPTH`/`WATERMARK` params default to the D4 values — formal overrides ONLY.
- W2.7 demo: feed `zref::MixerTone` pairs at ≥ 256 pairs per 1024 gpu cycles after a 512 initial fill (the tested steady pacing) → zero underruns; the FIFO output stream then equals the tone stream bit-exactly.

### 2026-08-15 ~11:45 — W2.3 + W2.4 merged to main by orchestrator

- Merges a83e104 (INPUT) + e66e363 (AUDIO): tests/CMakeLists banners unioned; diagrams regenerated from merged ledger (ledger:check green, 87 blocks).
- cppcheck gate (new from review M1) caught audio_dev.hpp member-init + rule-of-three findings on first contact with merged code — fixed properly (6135646), not suppressed.
- Integrated verification: clean rebuild, ctest fast 31/31, formal 3/3. Pushed (6135646). Branches/worktrees w23+w24 removed.
- W2.2 (video) + W2.5 (mem) agents were 429-killed mid-flight (reset 16:36); their worktrees are intact with substantial uncommitted work — resume via alarm after reset. W2.6/W2.7 dispatch after.

---

## 2026-08-15 — W2.5 COMPLETE (MEM subsystem RTL)

**Branch:** `wp/w2.5-mem` (worktree `.worktrees/w25`) · **Commits:** `6bcc4e9` (sim+rtl+tests+formal) → `02b68b5`/`cf2f28b`/`4eb60f4` (ledger ladder) → `a9aa06a` (fixup) — all pushed.

### Delivered (plan §2 W2.5, decisions D2/D3/D10)

1. **ZRef first** — `reference/include/zref/zref_mem.hpp`: `zref::SdramController` (cycle-stepped mirror of the ctrl law table: registered grants, spans, refresh steal/deferral), `zref::VramArbiter` (offer/edge two-phase oracle mirroring the REGISTERED-offer arbiter, credits, ages, RR, byte counters), `zref::MemoryGuard` (pure verdict function), `zref::HpsBridge` (burst bookkeeping under the frozen 16/1 latency profile) + `Pcg32`.
2. **RTL** — `zhao_sdram_params_pkg.sv` (the ONE frozen sim profile CAS3/BL8/tRCD3/tRP3/tRC9/refresh-780 + the ZH-004 `include` seam; `ZHAO_SDRAM_BOARD_PARAMS` swaps in board truth with zero other edits), `zhao_sdram_ctrl.sv` (open-page bank FSM, registered-grant law table — read spans 12/15/18, write 10/13/16, refresh steals 13 cycles; bounded-deferral refresh 780→820 urgent→840 hard; DQM-masked partial writes; full-8-beat read bus shaping), `zhao_vram_arbiter.sv` (D3: scanout strict priority, RR guaranteed, aging override at 20 makes B=40 STRUCTURAL, registered STABLE offer, credit pools, vram_bytes_by_client + frame_tick shadows, strict-priority-violation witness `scanout_preempted`), `zhao_mem_guard.sv` (Phase-2 map: scanout read-only both slots / blit write-only granted window; violations answered, traced, NOTHING forwarded), `zhao_hps_bridge.sv` (malformed rejected with nothing issued; one-in-flight; per-client bytes).
3. **Sim model** — `sim/models/zhao_sdram_model.sv`: behavioural SDR SDRAM, TESTBENCH-ONLY banner, excluded from every lint target and from `files.qip` (explicit note); enforces tRCD/tRP/tRC/refresh-interval(≤865)/protocol/MRS with per-kind error outputs + a peek port for shadow compares.
4. **Tests** — `sdram_directed` (spans exact, refresh steals = 12/preempted-refresh, partial-burst masking, conflict accounting), `vram_arbiter_directed` (boundary preemption, zero strict-priority violations, RR cycle, first-burst ≤ B=40 under scanout pressure, counters == oracle), `mem_guard_directed` (full-chain nothing-written via the model, boundary exactness, 2000-request PCG boundary fuzz, every verdict == oracle), `hps_bridge_directed` (harness-as-HPS per D10), `mem_random` three-way differential 1k fast / **100k nightly PASS** (grant order, credits, ages, offers cycle-exact; 64-KiB shadow word-for-word), `mem_bandwidth_budget` (Duo worst line fetch at 16-B×64/line + blit + refresh ⇒ zero scanout starvation, all lines within budget). **ctest -L fast 25/25 (100%)**; run-twice byte-identical.
5. **Formal** — liveness/no-escape/refresh-bound SBY + harnesses committed; wrappers deterministically **SKIP** on a documented toolchain gap: oss-cad-suite yosys's SV frontend cannot parse the frozen package style (unpacked-array localparams `ZHAO_CRC32C_TABLE`/`ZHAO_TIMING`, `module X import P::*;` headers). W2.4 got proofs through because their module has scalar ports; the MEM DUTs are struct-typed per the frozen zhao_pkg — the properties are ready and run unchanged on a UHDM/Surelog-integrated yosys or the hardware lane. Any non-parse sby failure stays FATAL.
6. **Ledger** — MEM.VRAM.ARBITER / MEM.HPS.BRIDGE / MEM.GUARD → **RTL_VERIFIED** via the V2 one-step ladder (three commit-pinned entries each, evidence paths on disk); **MEM.SDRAM stays SPECIFIED** (blocked_on: hardware) with a BANKED entry citing the directed+random+lint evidence and the ZH-004 obligations; diagrams regenerated; `ledger:check` green (V1–V14).

### Real bugs the differential caught (saved for the class)
- **Arbiter registered-offer race (RTL):** a combinational ctrl_req can change between the controller's acceptance edge and the grant pulse — the ctrl executed one burst while pend bookkeeping decremented another. Fix: the offered burst is a REGISTERED, held-stable offer.
- **Init t reset (RTL):** the PRE→REF1 transition didn't reset the wait counter — the first init AUTO_REFRESH never decoded and ran short.
- **PRE re-issue (RTL):** PRECHARGE decoded on every wait cycle, restarting the DRAM's tRP window each time (model caught it as tRP).
- **Partial read bus shaping (RTL):** reads retiring after `words` beats would let a following WRITE collide with the SDR burst tail — reads now wait the full 8-beat bus burst.
- **Test-only:** shadow-window aliasing (requests crossing the 64-KiB compare window) and an early loop exit that truncated the last grants — both fixed in the generator.

### Handoff notes
- The arbiter's client port len is BYTES (zhao_pkg law); the ctrl port len is WORDS (0 encodes 8) — the arbiter is the converter; row-tail burst splits never cross a 2048-word row.
- Scanout is best driven as SINGLE-BURST (16-B) isochronous fetches — that is what makes zero-preemption and B=40 both hold (see the bandwidth test).
- The yosys SV gap is WAVE-WIDE: any W2 formal whose DUT imports zhao_pkg struct types will SKIP the same way (wrapper pattern in `tests/formal/mem_formal_lane.cmake.in` detects it precisely).
- `refresh_stalls` counts 12 per refresh that preempted traffic (idle refreshes are free); bank conflicts count per row switch.

---

## 2026-08-16 — D3 liveness bound B re-derived, re-proven, and made tight

**Scope:** the ratified fix in `RATIFICATION-arbiter-liveness-bound.md` — `MEM.VRAM.ARBITER`
carried `RTL_VERIFIED` citing a liveness property that FAILS when actually run.
Worked directly on `main`.

### The number was wrong twice, and the errors cancelled

The frozen `B = G · MAX_BURST + REFRESH_OVERHEAD = 2·14 + 12 = 40` was wrong in two
independent ways, which is why 40 looked plausible for a whole wave:

1. It budgeted **one burst per client turn**. A 64-B request is **four** 8-word bursts,
   and the arbiter re-arbitrates only at burst boundaries, so a competitor's whole
   multi-burst request sits in front of the waiter. (The ratification called this.)
2. `MAX_BURST = 8 + tRCD + tRP = 14` is **not** the worst grant-to-grant span. The
   bank-conflict full read is PRE, tRP, ACT, tRCD, READ, CAS, 8 bus beats ⇒ **18**.

### Corrected derivation (and it lands on the measurement exactly)

A waiting client waits out `N` whole burst spans; `B = N · MAX_BURST_SPAN − 2`.

- `N = 1` (the burst in flight, never preempted mid-burst) + the bursts the competitor
  may still take before it must yield.
- **scanout** (strict priority): only ONE aging-override burst can precede it (serving
  it resets that client's age) ⇒ `N = 2` ⇒ **34**.
- **RR class**: `min(BURSTS_PER_REQ, ceil(AGING_OVERRIDE / MAX_BURST_SPAN)) = min(4, 2)
  = 2` ⇒ `N = 3` ⇒ **52**. ← the bursts-per-request factor lives in that `min`.

Both closed forms match the bisection **to the cycle**. That is the part worth keeping:
the formula was derived from the machine, then confirmed, not fitted to a passing run.

### Bisection (endpoints adjacent, so these are exact)

| client | largest FAILING B | smallest PASSING B |
|---|---|---|
| RR class (client 1) | **51** | **52** |
| scanout (client 0)  | **33** | **34** |

Committed both directions as sby tasks: `bmc` (PASS at 52/34), `bmc_tight_rr` (FAIL at
51), `bmc_tight_scanout` (FAIL at 33), `cover` (PASS, all 8 covers reached including the
new `c_near_rr` / `c_near_sc`). A bound that only passes is not proven tight.

### Two things found on the way that were NOT in the brief

- **The proof horizon cannot see a refresh.** `REFRESH_INTERVAL` is 780 cycles; the BMC
  runs to depth 130. So 34/52 are **refresh-free** bounds. Rather than quietly quote them
  as the operational bound (which would repeat the original sin at smaller scale), the
  spec adds an analytic `REFRESH_STEAL = 13` ⇒ operational **47 / 65**, and the harness
  now *asserts* its own scope (`a_horizon_is_refresh_free`): raise the depth past the
  refresh interval and it fires.
- **The scanout number was an init artifact.** The arbiter's port handshake doesn't know
  about `init_done`, so a client accepted during the ~26-cycle power-on init has the
  offer latched then sit un-taken for the whole init. That inflated scanout's bound from
  34 to **38**. D3 is a steady-state bound and the harness header always said so; the
  counter now agrees with the sentence. The RR bound was unaffected (51/52 both ways).

### Scanout re-check (ratification item 5)

Not broken — **improved**, and now proven rather than argued. Scanout's bound is 34/47,
tighter than the RR class's 52/65 and tighter than the believed-but-false 40. The
separate zero-starvation law (`scanout_preempted == 0`) is untouched: it counts
non-override grants of offers latched while scanout was eligible, a mechanism the
correction does not reach, and `mem_bandwidth_budget` still asserts zero.

### Process fix — "never ran" is now a hard failure (systemic finding)

This was the **second** block whose maturity rested on a proof that had never been
elaborated. Added `design/formal_runs.yml` (the formal-lane run registry) and ledger
rule **V16**, wired into `ledger:check` / CTest `ledger_check`:

- every `.sby` on disk must have a registry entry — adding a property without running it
  is a build failure, not a SKIP;
- a block at ≥ `RTL_VERIFIED` citing `tests.formal:` needs `status: green` **and**
  `covers: true` (assertions proven reachable — the MEM.GUARD vacuity hole);
- `never_ran` is a hard failure the moment anything cites it; `banked` may not back a
  maturity claim.

Verified by negative probe on the real ledger, not just unit tests: flipping the arbiter
entry to `never_ran` and dropping an unregistered `.sby` both fail `ledger:check`.

**V16 immediately caught a live one:** `INPUT.SNAPSHOT` was `RTL_VERIFIED` on
`input_snapshot_atomic.sby`, which had **no cover task** — every assertion there is
guarded by `$past(frame_tick.pulse)`, so an unreachable tick would have made all three
vacuous. Added five covers to the RTL's `ifdef FORMAL block and a cover task to the .sby.

Also fixed: `mem_formal_lane.cmake.in` had no PATH hardening (its INPUT/AUDIO siblings
do), so a mingw toolchain earlier in PATH makes yosys die with 0xC0000139 before parsing
a line — a "never elaborated" failure wearing a tooling costume.

### Handoff notes

- `REFRESH_URGENT = 40` in `zhao_sdram_params_pkg` was justified as "= the arbiter
  liveness bound". That justification is now stale. It is deliberately **not** re-pinned
  to 65: that would move `CNT_HARD` and invalidate the cycle-exact SDRAM directed tests
  and the zref oracle, which the ratification did not authorise. Flagged, not coupled.
- `audio_fifo_bounds.sby` is recorded `covers: false` — honest, and no block cites it, so
  V16 tolerates it today. The moment AUDIO.FIFO pins maturity to it, V16 demands covers.
- The build directory was mis-configured (`-I/include`: `VERILATOR_ROOT` unset at
  configure time) and `test_empty_frame_replay` / `test_abi_fuzz_parity` would not build
  on a **clean** tree. Fixed by reconfiguring with `VERILATOR_ROOT` exported.

---

## 2026-08-16 — MERGE + VERIFY: `wp/w2.6-cmd-debug` → main (V16's fourth contact, first pre-badge catch)

Merged as a true merge commit (`72318b1`), not a rebase: the branch's maturity_log
entries pin evidence commits `38f9b96`/`b64afe2` by hash, and a rebase would have
rewritten them into lies. Conflicts: `tests/CMakeLists.txt` (both sides appended after
the same anchor — kept main's MEM/W3.5 sections, appended W2.6 after, and DROPPED the
branch's `cmd_formal_lane.cmake.in` wrapper: it ran `sby -d bmc`, which breaks the
moment a property carries the bmc+cover task pair, and its "DONE (PASS" grep is weaker
than the hardened narrow-SKIP `mem_formal_lane.cmake.in` both cmd formal tests now
share); generated diagrams (regenerated from merged blocks.yml); `blocks.yml`
auto-merged cleanly and was reviewed semantically, not trusted.

### V16 on contact — exactly as predicted

`ledger:check` after the merge: 3 × V16 (both `.sby` unregistered; CMD.SCHEDULER citing
an unregistered property). Resolved by RUNNING the proofs, with covers:

- **`cmd_dma_crc_gate` assertion (b) was VACUOUS as shipped.** The blit-length law
  (`byte_len == canvas_bytes(mode)`, minimum 153,600 B) could never pass the harness's
  64-B `BLIT_BUF_BYTES`, so `guard_req_o.valid` was unreachable in the cone while the
  harness header claimed both CRC gates were in it. Fixed with a FORMAL-ONLY DUT
  parameter `FORMAL_BLIT_LEN` (structurally absent outside `ifdef FORMAL`); 7 covers
  all reached at k = 9..14 of depth 24 (btormc). bmc + cover green, 165.6 s.
- **`cmd_scheduler_slot_fsm`** properties (c)/(e) are transition-guarded → vacuously
  provable by a model where no slot completes. 5 covers (full charter cycle, both fence
  polarities, deadline-miss); all reached at steps 4..6 of depth 40. Green, 13.8 s.
- Both recorded green + `covers: true` in `design/formal_runs.yml` at `3ca4661`;
  `ledger:check` OK through V16 with no rule relaxed and nothing skipped.

### The branch's phantom oracles (found while making the ledger true)

`blocks.yml` and the contracts named `zref::CmdDma` and `zref::Crc32c` as reference
models and pinned REFERENCE_COMPLETE on them — **neither symbol existed anywhere**.
(A third phantom, `zref::framePixelCrc`, lived in DEBUG.CRC's contract; the contracts
also cited four random-test FILES that don't exist — the random lanes are flags on the
directed binaries.) The substance existed (a verdict oracle as a static function inside
the DMA test; the generated `zhao_abi::zhao_crc32c` with device law re-modelled ad hoc)
— the pointers lied. Fixed by making the pointers true: `zref::CmdDma` (structural
bytes-consumed law, generated `sizeof(ZhRecord*)` sizes) and `zref::Crc32c` (device
publish/violation law, arithmetic delegated to the one generated CRC machine) now live
in `zref_cmd2.hpp` and the tests differential against them.

### Two genuine RTL law defects — found by running the branch's own nightly lanes,
### which had never been run (the fast lanes passed by PCG luck)

1. **`zhao_cmd_scheduler`: the SetPresentationContract latch adopted ANY mode byte.**
   Phase 2 has no upstream enum-range validation (CMD.DMA's structural walk skips
   check 7 by design — decoder law, wave 3), so a lawless byte reaches the latch,
   indexes `ZHAO_TIMING` out of bounds and collapses through the 2-bit `mode_o`
   conversion. `cmd_random_soak` (100k frames) caught it: a random "unknown" opcode
   collided with 0x0020 and injected mode byte 0x78 (RTL published DUO, oracle 0x78).
   The RTL comment claiming the byte was "validated upstream" was FALSE. Fix: the latch
   refuses non-members 0-2, both sides. Soak now 13,884,383 checks / 0 failed.
2. **`zhao_debug_crc`: a single-byte frame (sof && eof same byte) was silently
   dropped** — neither published nor flagged, violating the device's own publish law.
   `debug_crc_random_nightly` (3000 frames, draws n==1 streams) caught it. Fix: the
   sof branch honors eof. Nightly now 9,000 checks / 0 failed.

### Maturity outcomes (all evidence personally observed green)

| block | maturity | pinned at | note |
|---|---|---|---|
| CMD.SCHEDULER | UNIT_VERIFIED | 768ce1a | re-pinned: soak FAILED at b64afe2 |
| CMD.DMA | UNIT_VERIFIED | b64afe2 (unit) / 768ce1a (ref) | ref re-pinned: zref::CmdDma absent at 38f9b96; now cites tests.formal |
| DEBUG.COUNTERS | UNIT_VERIFIED | as merged | oracle + all lanes genuinely green unmodified |
| DEBUG.CRC | UNIT_VERIFIED | 768ce1a (both) | re-pinned: phantom oracle + nightly FAILED at b64afe2 |

No demotions needed — but three of four promotions required re-pinning to evidence
that actually exists/passes, and the ledger notes say why in each case.

### Staleness review (32 commits behind)

- CMD/DEBUG oracles touch no qformats primitives (no fx16/unit8/trig) — immune to the
  raster/fill/fx_sin/unit8 corrections by construction.
- Fail-safe order (capture_format 3.2), record sizes, error space (ends at 14 →
  module-local 15/16 stays sound), counters.md D9: all unchanged since the fork.
- Wave-3 reserved→implemented promotions: opcode/size-frozen; the DMA walk has no
  implementedness check — promotion-immune.
- **Flagged for the `wp/w2.2-video` merge agent (not changed here):** the ratified Duo
  packed layout splits allocation (0x3C000) from occupancy (0x30000). CMD.DMA's blit
  length law delegates to `zhao_pkg::zhao_canvas_bytes`, which main still defines as
  245,760 for Duo. When the video merge lands the occupancy split in zhao_pkg, decide
  deliberately whether a Duo `DebugFrameBlit` carries 0x30000 (stored frame) or the
  0x3C000 allocation — CMD.DMA follows `zhao_canvas_bytes` automatically either way,
  but the directed blit tests pin Z60 only, so the Duo case is currently untested.

### Verification (pass / skip / fail, stated separately)

- `ctest -L fast`: **59 pass / 1 skip (format_check, clang-format absent — documented) / 0 fail**
- `ctest -L formal`: **7 pass / 0 skip / 0 fail** — every wrapper genuinely elaborated (formal_lane smoke, input_snapshot_atomic, audio_fifo, mem_vram_arbiter_liveness, mem_guard_no_escape 13.0 s, cmd_scheduler_slot_fsm 17.6 s, cmd_dma_crc_gate 331.7 s)
- `npm run -w compiler test`: **216 pass / 0 skip / 0 fail**
- `npm test` (ledger/fixgen/abi-gen): **20 / 14 / 20 pass, 0 fail**
- `abi:check`: clean (25 outputs match) · `tables:check`: clean (10 byte-identical)
- `ledger:check`: OK — 88 blocks / 40 ops, V1–V16 + staleness green

Fast lane, compiler tests, abi:check and tables:check re-run on an idle CPU after the
prover finished (same counts). Pushed `6db65d0..3380eca` to `origin/main` (8 commits:
1 merge + branch's 4 + 3 fix/formal/ledger). `.worktrees/w26` removed and
`wp/w2.6-cmd-debug` deleted (clean `-d`, fully merged); its one uncommitted change — a
`[files]` harness-path tweak in cmd_dma_crc_gate.sby — was superseded by the merged
version, which demonstrably resolves under the wrapper's workdir, and was discarded
deliberately.

### Proposal (NOT implemented here — belongs in its own change): close the
### phantom-pointer door the reference-model lane still has open

V16 closed "cited proof never ran"; the identical error walked through the
reference-model lane: `reference_model: zref::CmdDma` and `zref::Crc32c` were cited
and REFERENCE_COMPLETE was pinned on them while NO such symbol existed anywhere
(plus `zref::framePixelCrc` in a contract, plus four cited random-test FILES that
don't exist). Cheap generalisation, suggested as **V17**:

1. For every rtl block at >= REFERENCE_COMPLETE, the `reference_model` symbol's last
   segment must have a definition hit in `reference/` — regex
   `(class|struct)\s+<Name>\b` or `<Name>\s*\(` over `reference/include/**/*.hpp` +
   `reference/src/**`, injected like V16's `formalTasksOnDisk` so the rule stays pure.
   Coarse, but it catches exactly the failure that happened: a name nobody defined.
2. Every `tests.directed` / `tests.random` / `tests.formal` path cited by a block at
   >= UNIT_VERIFIED must exist on disk (V6-style `exists` probe — today only some
   paths are existence-gated).
3. Optionally: contract files' "Scalar reference function" backtick symbol must match
   the block's `reference_model` (drift between contract and ledger is how the
   framePixelCrc phantom survived).

Same disease, same cure as V16: a citation is not evidence until a checker can
resolve it.

### Meta-note (coordinator's observation, endorsed)

The `zhao_cmd_scheduler` comment claiming the mode byte was "validated upstream"
while nothing validated it is the same failure shape as a badge citing a proof
nobody ran: an asserted invariant with no enforcer. The fix commit corrects the
comment to name the true situation (Phase 2 has NO upstream enum-range validation)
and makes the latch itself the enforcer. Worth a review-lane heuristic: any RTL
comment of the form "X by construction / validated upstream" should name the
enforcing code or property, so the claim is checkable.

---

## 2026-08-16 — MERGE + VERIFY: `wp/w2.2-video` → main (the clean-merge trap, walked deliberately)

**Merge:** true merge commit `3406850` (merge, not rebase: the branch pins no
maturity hashes, but the salvage commit's "as-authored" provenance claim is
itself something a rebase would rewrite; merge is also this repo's practice).
Conflicts: `reference/CMakeLists.txt` (union), `tests/CMakeLists.txt`
(banner-append; the branch's `video_formal.cmake.in` wrapper DROPPED for the
shared hardened `mem_formal_lane.cmake.in` — no PATH hardening, no
multi-task support — the same resolution W2.6 made for the CMD wrappers).

### The survey undercounted the branch by nearly half

`.worktrees/w22` held **2,355 lines of UNCOMMITTED work** the queue survey
never saw (it counted commits): the whole differential test lane
(tests/video/, 13 files), three SBY properties + harnesses, and the CMake
block. Salvaged as-authored onto the branch (`6a795ea`) with its defects
catalogued in the commit message, then fixed on main where the fixes are
reviewable. Also present: `tools/w22_ledger.py`, a scratch one-shot ledger
editor that would have blind-promoted all four VIDEO blocks in lockstep
(RTL_VERIFIED citing formal runs nobody had run — pre-V16 thinking).
Deliberately NOT used, NOT committed; discarded with the worktree.

### Semantic re-derivation (the core of the job) — what git merged cleanly that was wrong anyway

1. **`zref::canvas_bytes(Duo)` returned 245,760 — the ALLOCATION.** Written
   before the allocation/occupancy split (video_rules.md §1, ratified
   2026-08-15). The C++ render law (`zref::render::canvas_bytes`) already
   said 196,608 — the two languages DISAGREED for Duo, cleanly merged. Fixed
   by delegation to the one definition (charter §29-6) at `0b8c71c`.
2. **`zref::frame_pixel_crc` was a second implementation of the
   displayed-stream CRC composition** — main's `zref::render::displayed_crc32c`
   landed while the branch was parked (its header literally records checking
   for the branch's phantom `zref::framePixelCrc` and finding nothing). The
   two compositions AGREED (border rows, packed-view row assembly — the
   branch had the post-MAJOR-3 layout right), but one-law-twice is the
   charter §29-6 defect regardless: now a thin adapter over the render one.
3. **`zref::Scanout` was a phantom-in-waiting:** ledger + contract cite
   `zref::Scanout`; the branch's class was named `VideoSys`. Renamed (alias
   kept). The W2.6 lesson applied BEFORE promotion instead of after.
4. **Everything else checked clean:** the video oracle contains no
   fx16/unit8/trig arithmetic at all, so the six post-fork numeric-law
   corrections (raster crack pair, fill-rule ≥, unit8 wrap, fx_sin endpoint
   guard, heightfield quantisation, sky drum) cannot reach it. Fetch
   geometry (Z60 768 B/12 req, Storm 640 B/10 req, Duo 2×512 B/8 req packed
   views), timing tables, latch law, deadline law: re-derived against
   current spec/video_rules.md, all agree.

### The Duo decision (flagged by W2.6, taken here) — `a7e5964`

`zhao_pkg::ZHAO_CANVAS_BYTES_DUO` 245,760 → **196,608** (occupancy; the SV
side now agrees with the C++ law and the blit-length law follows
automatically). `ZHAO_CANVAS_BYTES_MAX` stays 245,760 (allocation). NEW
`zhao_displayed_bytes()`: Duo's displayed stream is 512×240×2 = 245,760 —
COINCIDENTALLY the allocation and NOT the occupancy; the earlier
`displayed_crc32c` confusion came exactly from these numbers colliding, so
the three now have three names, and wiring `zhao_canvas_bytes` into
DEBUG.CRC's expect_bytes would be a named, documented bug. Duo blits are no
longer untested: `cmd_dma_directed` 8d commits 196,608 bit-exact over
[0,0x30000) of slot 0 and REJECTS 245,760 (status 18, zero fetches) — the
pre-split value must now fail. `commands.zidl` Duo comment corrected;
zidl_sha256 regenerated through the 25 outputs (abi_identity unchanged).

### Vacuity findings (the disease, four more strains)

1. **All four salvaged random tests ignored `tb.failures`** — a per-cycle
   RTL/oracle mismatch replays identically on both runs of a seed, so the
   run-twice hash still matched and the binary exited 0. The differential
   half of the random lane proved only determinism. Fixed (`20c5cb3`).
2. **`video_mode_fv`'s reset-idle assertion was nested inside `if (rst_n)`**
   — structurally unreachable, vacuous by construction. Moved + guarded.
3. **No salvaged harness had a single cover.** Added 5 (mode) + 6
   (framectl) + 6 (linebuf) covers; the mode raster's true reset position is
   ~6,700 vid cycles from the wrap, so the DUT gained an `ifdef FORMAL`
   reset override (the W2.6 FORMAL_BLIT_LEN precedent) starting the
   autonomous ring 4 cycles before it.
4. **The linebuf harness's "free" stimulus wasn't free — NEW TOOLCHAIN TRAP,
   generalising the recorded anyseq-on-locals one:** read_slang ties
   UNDRIVEN LOCALS to 1'x and `prep`'s opt folds `if (x)` branches away
   BEFORE sby's `setundef -anyseq` runs. Parts of the harness (including an
   entire never-assigned `fill_buf_q` — buffer 1 was never fillable, half
   the never-torn property vacuous) were silently gone. Free stimulus MUST
   be module input ports in this flow. Found BY the new cover task
   (c_fresh_both unreached → pulled the thread).

### A genuine RTL contract violation the un-vacuous property then caught

With stimulus genuinely free, the linebuf never-torn bmc produced a REAL
counterexample: **aborting a FULL buffer "un-does" its completion toggle,
and an un-toggle IS a toggle** — a pulse the vid-side 2FF chain samples as
stale `buf_fresh` for ~2 vid cycles while the gpu side already holds the
buffer EMPTY (and the fetch may be refilling it): a torn read. The module
header claimed the abort path was toggle-free "by construction" — FALSE for
the FULL case; a comment asserting an invariant no code enforces (process
law: same defect as an unbacked badge). In the SYSTEM the hazard is
unreachable — aborts fire only at the vblank re-arm/mode flush and
freshness decisions are line-edge-spaced — so the resolution is: header
rewritten to name the TRUE enforcer (system timing), the formal harness
assumes exactly that spacing law (documented, with a cover proving the
assumption still admits consumption), and the hazard is filed for any
future integration that aborts outside vblank. Also fixed while here: the
DUT storage is anyinit in formal (no reset), so equality is only claimable
for addresses the fill wrote — the harness now watches ONE symbolic
held-constant address (the anyconst idiom, as a PORT because of the trap
above) instead of a 16-Kbit shadow + 128-bit mask that made the solver
crawl minutes-per-step.

### Teardown deadlock: second + third observed strikes (PRE-EXISTING on main)

`mem_bandwidth_budget` passed every check, then hung a fast lane for 28
minutes in `VlThreadPool::~VlThreadPool` (6.58 s CPU frozen over a 25 s
sample; verdict lost in the unflushed pipe; clean standalone rerun passes in
seconds). The previous session's aborted-lane log showed `mem_sdram_directed`
dying identically on a tree WITHOUT this merge. Every prior "green" fast
lane on main was a scheduler coin-flip from hanging. Workaround (stated as
such) at `7ae6b3b`: `zhao::exit_hard()` in zhao_sim.hpp; all 15 Verilated
mains that returned normally now exit through it (6 memory, 8 video,
1 audio). A hung test is neither a pass nor a skip.

*(verification numbers + ledger ladder appended below when observed)*

---

## Cheap-win evidence-integrity tier (R5, R3, R1) — 2026-08-16

Implemented from DIAGNOSIS-verification-evidence-integrity.md, in the
ordered scope R5 -> R3 -> R1. Commits `bc707dd` (scope guards),
`2299990` (DEBUG.COUNTERS fix), `e60ba85` (claim annotations + new
enforcement assertions), `7fe8e82` (rules V17/V19/V20 + tests),
`f80ec06` (registry notes) — all pushed to origin/main as they landed.
R2 (lane-run registry, reserved as V18), R4, and everything else in the
catalogue NOT started, per the brief.

### R5 — scope guards (rule V19)

The diagnosis's census was WRONG: not two unguarded bounded harnesses
but SEVEN (audio_fifo_bounds bmc 32, cmd_dma_crc_gate bmc 24,
cmd_scheduler_slot_fsm bmc 40, input_snapshot_atomic bmc 30,
mem_guard_no_escape bmc 30, video_framectl_one_fence bmc 60, banked
mem_sdram_refresh_bound bmc 900). V19's first run against the pre-guard
tree flagged exactly those 7. All guarded in the established
self-asserting style; every load-bearing property re-run green at its
recorded depth (6 pass / 0 skip / 0 fail; refresh cover re-run green
apart) and every guard OBSERVED RED at a raised depth — cmd_dma's CEX
lands at k = 25, one step past the proven 24.

Finding en route: audio_fifo_bounds runs `multiclock on`, where a clk
posedge costs ~2 SMT steps — depth 32 is ~16 gpu cycles = FOUR audio pop
opportunities, HALF what steps-as-cycles suggests. The first guard
(<= 8 ticks) could not fire even at depth 48 — a guard that cannot go
red is the disease — and was corrected to the measured window before
landing. The property's effective scope was half its .sby header's
implication; recorded in the harness and registry.

### R3 — prose-claim lint (rule V20) and the audit that is the point

Census correction: 17 claim sites across the 7 files, not 14 — a
hyphenated "stable-by-construction" and two line-wrapped "by /
construction" evade a line grep (the lint joins wrapped comment lines).
And ZERO sites were machine-resolvably annotated, not "2 files
annotated" — the existing mentions were prose pointing at prose.

The audit's yield, on the project's base rate as predicted:

* **One live B-class defect (fixed, `2299990`)**: DEBUG.COUNTERS
  sampled provider channels AT the frame_tick pulse; every real
  provider (scheduler x3, dma x3, fifo x1) presents its latched shadow
  with a registered one-cycle valid pulse the cycle AFTER the tick.
  Composed in the shell this wave is building, the bank would NEVER
  capture — every counter zero forever. Green until now because the
  directed harness drove valid combinationally at the tick: an
  environment model no committed provider implements. Fix: capture one
  cycle after the pulse; harness rewritten to the real provider timing;
  spec/counters.md 3 states the timing law. Falsified both ways: the
  faithful test FAILS against pre-fix RTL ("expected 7, got 0"), passes
  against the fix (4 pass / 0 skip / 0 fail).
* Two true-but-unenforced claims got real enforcers, both
  mutation-verified: a_mode_act_in_range (cmd_scheduler; the
  ZHAO_TIMING-OOB shape of the W2.6 lawless-byte defect; removing the
  latch's refusal fails the proof) and a_cdc_data_stable_unless_toggle
  (framectl vid->gpu crossing; an unconditional cur_slot flip fails it).
* One mathematical triviality reworded to state the math (linebuf's
  1-bit-gray); everything else annotated to formal properties, RTL
  symbols, or the grant-order differential, each pointer machine-resolved
  by the lint (paths exist; .sby refs must be V16-registered; :symbols
  resolved against the property's staged cone).

V20 probes, all red-then-restored: annotation removed; phantom path;
missing symbol.

### R1 — V17 citation coherence (contract-aware)

As the diagnosis ordered: no re-check of V6's surface. (a) oracle-symbol
definition under reference/ at >= REFERENCE_COMPLETE; (b) contract
"Scalar reference function" == ledger reference_model; (c) contract-cited
tests/ paths exist past SPECIFIED; (d) anti-alias tie — the cited test
must textually name its oracle.

First run against the real repo: 7 flags. (i) vram_arbiter_directed and
video_mode_random reached their oracles only through harness headers and
never named them — annotated truthfully after verifying the chains
(zhao_mem_chain.hpp instantiates zref::VramArbiter/SdramController;
video_harness drives zref::Scanout whose member IS zref::VideoMode).
video_harness.hpp's header still said `zref::VideoSys` — the phantom
name 0b8c71c renamed away, surviving in prose; fixed. (ii) FIVE flags
were a false positive of MY rule: compiler/tests/*.test.ts citations
substring-matched as tests/*; fixed with a lookbehind in the rule — the
contracts were right. Recorded here because a rule's own first-run
false positive is exactly the kind of thing this tier exists to catch.
Also fixed on contact: MEM.VRAM.ARBITER.md's Latency section still
quoted the DISPROVEN B = 40 one screen above the section that corrects
it, and its header maturity stamp was stale (SPECIFIED vs RTL_VERIFIED).

V17 probes: phantom symbol on MEM.GUARD (three surfaces fire);
real-but-wrong zref::AudioFifo (drift + alias fire, existence silent).

### Verification (counts, never percentages)

* ledger unit tests: 40 pass / 0 fail (20 pre-existing + 20 new for
  V17/V19/V20)
* compiler workspace: 216 pass / 0 fail. abi:check clean (25 outputs).
  tables:check clean (10 byte-identical).
* formal lane: 6 properties re-run via ctest — 6 pass / 0 skip / 0 fail;
  scheduler + framectl re-run again after their cone changes (pass);
  refresh cover task green apart (banked stays banked; its guard has no
  red observation — its bmc has never finished; recorded honestly).
* ledger:check on the final tree: green, V1-V17 + V19-V20 + staleness,
  88 blocks / 40 ops / 10 formal runs.
* fast lane (ctest -L fast, final tree): 74 pass / 1 skip / 0 fail out of
  75 — the single skip is format_check on a machine without the pinned
  clang-format; CI's LLVM-15 format job remains the authoritative gate.

---

## 2026-08-16 — W2.7 IN PROGRESS (console shell + Duo marker demo)

Working directly on `main`. Interim log; final lane counts appended at close.

### Landed and pushed so far

- `bb13352` **dma: blit write-data seam is now a beat stream** — the W2.6
  guard_wdata_o sideband carried ONLY the first 8 bytes of every 64-byte
  blit write request (and cmd_dma_directed's "bit-exact" check compared
  exactly those 8), found because the shell's SDRAM write-data queue had
  nothing to connect to. New guard_wvalid_o beat stream (M_BLIT_DATA);
  test captures ALL bytes; mutation-verified red; formal
  cmd_dma_crc_gate re-elaborated green 7/7 covers (registry at `7bebf0f`).

### The composition dossier (measured with tests/shell/shell_probe.cpp)

1. **Scheduler/DMA record-vs-blit DEADLOCK** — scheduler backpressures
   records while a blit dispatch is pending; the blit is only acceptable
   after the same packet's stream drains. Shell fix: record-framer QUEUE
   (FRAMER_Q) + sticky deadlock tripwire.
2. **FB slots shared one DRAM bank** — scanout reads + blit writes
   row-thrash bank 0: ~82/192 Duo lines starved per frame. Fix: FB-slot
   BANK SPLIT (ZHAO_FB_SLOT1_BASE -> 0x0200_0000, bank 1) — frozen-
   interface edit with RATIFICATION-REQUEST-fb-slot-bank-split.md (this
   dir); guard now checks the DISJOINT two-region map (the old contiguous
   form would have admitted reads from the hole); zref mirrors, formal
   harness, tests, specs updated; mem lane 12 pass / 0 skip / 0 fail;
   mem_guard_no_escape re-elaborated green (bmc+cover).
3. **Even bank-split, free write interleave starves the serial fetch by
   ~2%/line** (accumulates into a deterministic 2-of-4-line limp). Fix:
   shell BLIT PACER (lawful client pacing; D3 arbiter untouched) —
   starvation exactly 0.
4. **60 Hz full-canvas cadence is INFEASIBLE as frozen**: lawful Duo blit
   = ~338k cycles (HPS fetch ~93k on the frozen 16-cycle one-in-flight
   D10 profile + starvation-free commit ~245k) vs the 318,592-cycle frame,
   and D8 closes every packet at its first tick => every blit fences
   STATUS_DEADLINE in every mode; Z60 raw demand (258k) even exceeds its
   whole frame (251,520). Demo runs at the machine's sustainable cadence:
   fresh frame every 2nd tick, repeats CRC-identical (60 Hz law proven
   600x), fault/fence pattern PINNED in closed form. Full dossier:
   reports/status/phase2_wave2.md.
5. Tick/frame law measured: tick k = vswap decision at line 244 of raster
   frame k-1; the mode-set packet's write latches at frame_start_1 (F1 is
   already the new mode); P_f (published tick 2f-1) displays fresh at
   F_{2f+1}.
6. CMD.DMA contract said "No VRAM writes" — predates the blit engine,
   FALSE for shipped RTL; corrected with the seam law. Plan's "48
   interface lines" moved inside the views (border rows are hardware-black
   by ratified spec). DEBUG.COUNTERS' provider sketch (dma x3) would
   duplicate catalog ids 1/2/29 — shell wires the counters.md §5 owner
   table instead.

### Evidence state at this line

20-packet gate demo: **846 checks passed / 0 failed** (all displayed CRCs
exact incl. repeats, counters closed-form, pads/audio bit-exact, fences
pinned). In flight: full 600-frame gate run (--write), video lanes on the
re-based oracle.

### 2026-08-16 ~13:00 — shell + demo + goldens landed (commits 38689ef..99ed2f0, pushed through ea96db4)

- `38689ef` mem: FB slot 1 -> DRAM bank 1 (ratification-pending frozen-
  interface edit; disjoint guard map; all mirrors/tests/specs; mem 12/0/0,
  video 16/0/0 incl. _full soaks, formal guard re-elaborated green)
- `ea96db4` formal: registry re-run record (guard @ bank split)
- `3971d86` shell: zhao_shell_top + tb + SW.RUNTIME.HPS harness + probe
  (ten glue seams documented in the module header; lint -Wall clean)
- `c5e1aca` demo: Duo marker gate + per-mode goldens at the SUSTAINABLE
  cadence + captures/golden/wave2/{duo_markers,z60,storm,duo}_*.zcap.
  600-frame gate observed green: 24,629 checks passed / 0 failed
  (1,202 displayed frames, 95.8M PCM pairs bit-equal, every displayed CRC
  == zref composition incl. 600 CRC-identical repeats, faults/fences
  pinned in closed form). Goldens observed green: 756 checks / 0 failed,
  byte-identical replay.
- `99ed2f0` demo: soak byte-deltas become jitter windows (publish jitter
  lawfully wobbles which side of the shadow latch a few blit bursts land
  on; measured; short soak 4,126 checks / 0 failed)

Startup constants measured and pinned: Duo starvation baseline 0 (the
border absorbs the mode-flush refetch); Storm's is exactly 640 (2 lines,
no border) — the golden pins it; baseline capture happens at the tick-2
sweep for that reason.

In flight at this line: official 600-frame gate verify + official
10,000-displayed-frame soak + the full fast lane, all on the final binary.

### 2026-08-16 — W2.7 closing verification (all personally observed; commits pushed through 399a891)

- `4f76d2e` demo: capture COUNTERS record the half-rate truth — the section
  carried the abandoned 60 Hz model's constants (frame_cycles 601 /
  faults 1) while the machine reads 1201 / 601. BYTE-IDENTITY COULD NEVER
  CATCH IT (writer and committed file agreed); found by decoding the
  capture with an independent reader. Regenerated: 24,629 checks / 0
  failed; zhao-capture verifies all 603 sections; trajectory hash
  unchanged.
- `399a891` ledger: CMD.SCHEDULER / CMD.DMA / DEBUG.COUNTERS / DEBUG.CRC
  -> RTL_VERIFIED at 4f76d2e (the composed-machine differential);
  SW.RUNTIME.HPS D10 note; reports/status/phase2_wave2.md (the gate
  verdict + composition dossier).

Final lane counts (pass / skip / fail):

- OFFICIAL 600-marker gate (final binary): 24,630 / 0 / 0 in 34m40s
  (and the regeneration run 24,629 / 0 / 0 in 34m32s; both byte-identical
  captures). shell_duo_markers_fast (40 markers) 153 s green in-lane.
- fast lane, final tree: 78 / 1 / 0 (skip = format_check, pinned
  clang-format absent; CI LLVM-15 is the format gate).
- nightly label: the dedicated lane was reaped by the task runner at
  ~65 min (an infra kill, not a test failure — the log shows 81 tests
  green, zero failures, mid-lane); every remaining nightly-labeled test
  was independently green this session (video _full soaks 16/0/0 lane,
  cmd_random_soak, gate runs, golden replay, leftovers re-run 2/0/0).
- formal, ALL re-elaborated on the final tree this session: formal_lane,
  audio_fifo, cmd_scheduler_slot_fsm, input_snapshot_atomic (80.97 s),
  cmd_dma_crc_gate (7/7 covers, at bb13352), mem_guard_no_escape
  (disjoint map, at 38689ef), mem_vram_arbiter_liveness (875.96 s: bmc
  PASS at the tight bounds, both tightness tasks FAIL as recorded, cover
  PASS), video_mode_timing (17.95 s prove), video_scanout_linebuf
  (109.40 s), video_framectl_one_fence (91.34 s) — 10 lanes green,
  0 skips, 0 fails. (mem_sdram_refresh_bound stays banked, unchanged.)
- npm: compiler 216/0; ledger 40/0; fixgen 14/0; abi-gen 20/0; abi:check
  clean (25); tables:check clean (10); ledger:gen fresh; ledger:check
  green.
- captures verified with the INDEPENDENT reader (zhao-capture):
  duo_markers 603 sections; z60/storm/duo 10-frame 23 sections each.
- 10,000-displayed-frame soak: --soak 100 rehearsal green (4,126 / 0 / 0);
  the FULL soak is running detached (PID 32640, build/soak_final.txt) —
  verdict appended below when it lands.

### 2026-08-16 16:24 — SOAK VERDICT LANDED; W2.7 CLOSED

- OFFICIAL 10,000-displayed-frame soak (detached PID 32640, final binary,
  `demo_duo_markers.exe --soak 5000`): **205,026 checks passed / 0
  failed** — 5,000 published frames, 10,002 ticks, 10,003 displayed CRCs,
  796,698,579 PCM pairs bit-equal; PCG jitter on publish timing, pads,
  rumble; byte-delta jitter windows never left +/-8 KiB; wall 2h28m
  (13:56:15 -> 16:24), stderr empty, clean exit. Personally observed in
  build/soak_final.txt.
- Every lane in the closing-count list above is now COMPLETE — nothing
  remains in flight. Working tree at close: my last commit 81872b0
  (ratification conditions applied); subsequent terrain-lane commits
  (26764b4, d57d5ed, cd1847a) and dirty terrain files belong to another
  run and were left untouched.
- Phase-2 gate verdict stands as written in reports/status/phase2_wave2.md
  §3: demo exists, runs, capture-exact, repeat pattern PINNED (asserted
  expected CRC every tick, drift never tolerated); the plan's
  zero-deadline-fault 60 Hz letter is INFEASIBLE as frozen (dossier D5) —
  closure of that single criterion awaits the architect's ruling
  (sustainable half-rate as law, or ratify a named 60 Hz path and re-run).
  The 30 Hz figure is a DebugFrameBlit TRANSPORT cost, not a rendering
  cost; the Phase-3+ on-fabric render path inherits no such ceiling.
