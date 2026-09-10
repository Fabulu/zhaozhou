# FIELD program directory as a scanned memory — built, differential-gated, NOT yet mapped

2026-09-10 · branch `zixxtrixx-v8-closeout` · lane `fpga/rtl/field/` + its tests · **nothing committed, no fit run**

Roadmap: `Zhaozhou_ALM_Liberation_Roadmap_2026-09-09.txt` §5 ("THE FIRST
INDEPENDENT CHEAP REPLACEMENT: FIELD PROGRAM DIRECTORY") and §14 Commit3.

**Headline, stated as the roadmap requires:** the register-and-compare-in-parallel
directory `zhao_field_progcache` (map-only **2,237 est. ALM / 1,490 registers**, a
Quartus Analysis & Synthesis estimate, never fitted) has a candidate replacement
`zhao_field_progdir` whose rows live in one 16×80 synchronous memory with one hash
comparator and one stamp comparator, **transaction-identical to the oracle under
five elaborations and ~350,000 checks, with the checker seen to fail on a
committed tie mutant, at a MEASURED 20 clocks per transaction**. Its ALM cost is
**UNKNOWN**: no fit or map has run, and *no claimed numeric delta exists until
measured*. The one gate is named below.

---

## 1. What is in the working tree

| file | what |
| --- | --- |
| `fpga/rtl/field/zhao_field_progdir_scan.sv` | the scanner: unified request (lookup/commit + hash + verdict), kind echoed on the response; ENTRIES×(HASHW+LRUW) rows in ONE `(* ramstyle = "M10K" *)` memory, ENTRIES valid bits in flops, one `==`, one `<`; memory sheet in the header |
| `fpga/rtl/field/zhao_field_progdir.sv` | the adapter: the OLD two-interface port list, port for port; lookup priority; kind-routed response channels |
| `fpga/rtl/field/zhao_field_progcache.sv` | **the retained oracle — one default-preserving edit** (§6) |
| `tests/field/tb_field_progdir_diff.sv` | oracle and candidate side by side, same params; `-DPROGDIR_MUTANT` swaps in the mutant |
| `tests/field/field_progdir_diff.hpp` | the transaction-level differential engine (shared by gate and control) |
| `tests/field/field_progdir_differential.cpp` | THE GATE: directed, `--random N`, tie script, timing probe |
| `tests/field/field_progdir_mutant_control.cpp` | inverted-polarity positive control |
| `tests/mutants/zhao_field_progdir_scan_mutant.sv` | committed mutant: tie rule `<` → `<=` (one substantive line) + the adapter copied for wiring |
| `tests/field/progdir_compat/Vzhao_field_progcache.h` | one-line shim so the UNCHANGED real-program suite runs against the candidate |
| `tests/CMakeLists.txt` | +103 lines: 3 differential elaborations, control, re-pointed suite, 2 lints |
| `design/fit_targets.yml` | target `zhao_field_progdir`, gate **F-PROGDIR1**, `min_m10k: 2`, `max_registers: 700` |
| `design/prod_manifest.yml` | `zhao_field_progdir`, `zhao_field_progdir_scan`: `not-yet-adopted`; the old block stays in `top:` |
| `design/contracts/FIELD.PROGCACHE.md` | dated amendment: latency/throughput sections superseded; the caller's lifetime law stated |
| `design/blocks.yml` | **not touched** — another live lane has it modified; the ledger entry still names the oracle, correctly, since the oracle is still what is composed |

Every shape and constant is a named parameter: `ENTRIES`, `HASHW`, `LRUW` on the
scanner; `ENTRIES`, `LRUW` on the adapter and (now) the oracle.

## 2. The supplied file does not exist

§5 says *"SUPPLIED: rtl/zhao_rescue_progdir_scan.sv"*. A full search of the
zencrifice root (every directory, plus the contents of all three kit zips,
including `ZHAOZHOU_MEMORY_FIRST_RESCUE_EVIDENCE_AND_CHECKS_2026-09-09.zip`,
which has no `rtl/` directory at all) finds the name **only inside the roadmap's
own text** (its §5 and its own file listing at line 1584). Same as the
`zhao_rescue_arena3.sv` finding. Everything here is authored fresh against §5's
written contract; nothing was inherited from a file that was never delivered.

## 3. What was verified (instrument named per item)

| claim | instrument | result |
| --- | --- | --- |
| transaction identity with the oracle, ENTRIES=16 LRUW=48: fill, hit, LRU victim, rejected commit (full and empty), rejection not remembered, duplicate commit (caller misuse, old semantics retained), all four lookup×commit **interleaved pairs**, **held** lookup response while a commit lands (hit+rejected, miss+rejected: the §5 hazard), reset with work outstanding | `field_progdir_differential` directed | 1,176 checks, 127 ops, 0 fail |
| same, random ops (45 % lookup, 35 % commit ~22 % rejected, 10 % pairs, 8 % held, 2 % mid-flight resets, stalls 0–6) | `--random 200` | 121,206 checks, 12,146 ops |
| **LRU ties and stamp wrap** — constructible only when the counter wraps: at LRUW=2 hash 0 is hit until its stamp equals hash 1's, then a commit must evict slot 0 | ENTRIES=2 LRUW=2 directed | 504 checks incl. the tie |
| ties/wrap under random ops | ENTRIES=2 LRUW=2 `--random 300` | 110,075 checks |
| non-power-of-two directory + wrap | ENTRIES=3 LRUW=6 directed + `--random 300` | 1,110 + 114,421 checks |
| **the checker can SEE a tie fault** | `field_progdir_mutant_control` on the committed mutant | PASS: agrees on all 127 non-tie ops (every counter balancing — the weak-vector half), then FAILS at the tie op with **the commit slot alone** differing (mask 0x2), counters balanced; hits diverge two ops later |
| the real-program law (zref::field::ProgCache, 3 `.zprog` fixtures + corruptions) | `field_progcache_directed.cpp` UNCHANGED, re-pointed via the shim | 124 + 3,437 checks — the same suite the oracle passes |
| every counter fires on the candidate | explicit maxima checks in the directed run; the re-pointed suite's own `hits==2 / misses==1 / rejected==3` | all five moved |
| the elaboration guard fires | built at `-GLRUW=1` | `%Fatal ... LRUW must be >= 2 (got 1)` — lint alone would not have shown this |
| lint | `verilator --lint-only -Wall`, 4 tops | 0 diagnostics |
| Quartus-17 forms | `tools/quartus/check_quartus17_syntax.py` | clean, 223 files |
| memory inference obstacles | `tools/quartus/check_ram_inference.py` on both new files | "no structural obstacles" (static; not a fit) |
| manifest accounting | `tools/quartus/check_prod_manifest.py` | my two modules accounted; **1 error remains and it is another lane's** (`zhao_forge_cliff_ram`, FORGE) |
| uncashed cheques | `tools/budget/uncashed_cheques.py` | lists `zhao_field_progdir` **PENDING — fit target, never measured**, which is the truth |

### Where the differential is honest about its own limits
The two blocks run the same script *each at its own pace*; every op completes on
both sides before the next starts, so accepted ORDER is the script order by
construction and the comparison is at op boundaries. Pair ops check that both
sides accept the lookup first and never fire both on one edge. What this does
NOT exercise: a caller that offers a new request on a channel while that
channel's previous response is still untaken (illegal for the old block too —
its ready is low then), and true cycle-level interleavings that only the
candidate's longer occupancy makes possible. Those need a consumer, and there is
none (§5 below).

## 4. Measured cycle counts — derived from RTL, not inherited

Convention: the accepting edge is edge 0; "takeable at N" means the response
register is high after edge N−1 so a ready consumer takes it at edge N. Timing
probe in `field_progdir_differential` (`MEASURED` lines), ENTRIES=16, no stalls:

| | oracle | candidate |
| --- | --- | --- |
| lookup hit — takeable at | 1 | **20** |
| lookup miss | 1 | **20** |
| valid commit (evicting) | 1 | **20** |
| rejected commit (no scan) | 1 | **3** |
| accept-to-accept, lookup stream (hit or miss) | 1 | **20** |
| accept-to-accept, valid-commit stream (fill then evict) | 1 | **20** |
| accept-to-accept, rejected-commit stream | 1 | **3** |

Structure: 1 (accept) + 16 reads at one row per clock, one pipeline stage
(issue i, evaluate j=i−1) + 1 (last evaluate) + 1 (decide) + 1 (scanner response)
+ 1 (adapter's response register) = **20**; the scanner idles again the edge its
response is handed to the adapter, so the interval equals the latency. The
scan is a fixed full walk — hit and miss cost the same, on purpose (one
transaction, one length).

§5's *"approximately 35 clocks"* described a two-clocks-per-row first candidate.
This one is the one-stage `zhao_geom_pose_cache` scan idiom, and the 35 is not
quoted for it; 20 is. Its "128 such lookups" illustration would be 2,560 clocks
here — and **128 is not a production demand; no trace exists** (next section).

## 5. The lifetime finding — checked before building

**There is no caller.** Nothing under `fpga/rtl/` drives `lu_valid_i` except the
generated `zhao_prod_top.sv`, whose pins are an LFSR. `FIELD.SEQ.CORE.md` says
integration "needs the remaining op dispatch first"; `FIELD.SEQ.EARTH.md` says
programs and FPLANs *live in* this store but never acquires one. So the question
"does the implementation reacquire at every point?" has the answer: **no
implementation exists to reacquire anything**, and the production acquire demand
is **UNKNOWN**.

What DID exist was the contract's own model of the caller, and it is the thing
§5 warns about: FIELD.PROGCACHE.md said *"a program in use is re-acquired on the
cycle it is needed rather than held across a frame"* and promised *"a resident
program costs a single cycle to find"*. That sentence was only ever tolerable at
one cycle per acquire. At 20 clocks, per-point reacquisition on one Earth patch
(1,089 lattice vertices) would cost ~21,800 clocks against `FIELD.SEQ.EARTH`'s
10,416-clock allowance before a single point executes. **The repair is a law, not
an RTL change: acquire once per program per association and hold the slot** —
consistent with EARTH loading uniforms "ONCE per association". It is now written
in the contract amendment so the first consumer inherits it. This packet is the
right repair for the directory; the lifetime it depends on has to be honoured by
a consumer that does not yet exist.

## 6. The one edit to the oracle

`localparam int LRUW = 48` in the body became `parameter int LRUW = 48` in the
parameter list — same default, elaboration-identical, nothing else in the file
moved (`git diff --stat`: +7 −2, the rest is the comment saying why). Reason:
ties and wrap are unreachable with legal stimulus at 48 bits; at `LRUW=2` both
blocks reach them in six transactions, and no test-bench deposit into internal
state is needed. Review point: if the reviewer prefers the oracle byte-frozen,
the alternative is `--public-flat-rw` and depositing `lru_ctr` in both DUTs; I
chose the parameter because a knob is what the working laws ask for.

## 7. Memory arithmetic, shown

Row = HASHW + LRUW = 32 + 48 = **80 bits**; ENTRIES = 16 → **1,280 bits logical**.

Cyclone V M10K: 10,240 bits; simple-dual-port aspect ratios 256×40, 512×20,
1024×10, 2048×5, …; **widest port 40 bits**. 80 > 40, so the row splits across
**two** M10Ks side by side, each 16-of-256 rows used: 1,280 / 20,480 = **6.25 %**
utilisation. That matches the roadmap's own §3 layout table row ("16x80, 1 read +
phase-disjoint write → 2") and the M10K allocation it carries. The alternative is
an MLAB layout (`ramstyle = "MLAB"`: Cyclone V MLAB = 32×20 = 640 bits per LAB;
16×80 → 4 MLABs) — one attribute, 0 M10K, ~4 LABs of ALM-as-memory. The shipped
attribute is `M10K` per the roadmap's allocation; if the map shows MLAB is the
better trade, the fit rule `min_m10k: 2` is the thing that changes with it, on
purpose (so the choice is visible, not accidental).

Port discipline: one read every clock (address = scan index), one whole-row write
in `S_RESP`'s first cycle from registers that already hold the row (`{lru_ctr,
q_hash}` — the 80-bit write-data copy an earlier draft registered is gone).
Read and write states are separated by at least two edges; a simulation
assertion checks it. Payload never reset; valid bits reset; invalid rows never
consulted (every use of `rd_q` is gated on `valid_q[j]`); no partial write; no
same-address read/write can occur; no epoch trick on uninitialised RAM. That is
§3.1 item by item.

## 8. The bill — MEASURED / STRUCTURAL PREDICTION / UNKNOWN

**MEASURED (Verilator, this session):** everything in §3 and §4 — identity,
order, held responses, ties, wrap, invalid commit, reset mid-flight; 20 / 3
clocks; every counter fired; the checker fails on the tie mutant.

**MEASURED (Quartus map-only, inherited and re-verified as applying to today's
file):** the OLD block, 2,237 est. ALM / 2,689 comb ALUTs / 1,490 registers / 0
M10K, `rtlCleanAtHead: true`, `sourceCommit 7481711`, and
`git diff 7481711 HEAD -- zhao_field_progcache.sv` is empty (apart from today's
parameter lift) — so the row describes the current oracle. 1,490 registers is
consistent with the header's own 16×(1+32+48) = 1,296 plus counters and response
registers. The row carries `critical: 1` (one critical warning) whose text is not
in the repo; unknown what it was.

**STRUCTURAL PREDICTION (by counting the RTL, not by tool):** candidate
flip-flops ≈ 16 valid + 48 `lru_ctr` + 128 counters + 5 occupancy + 32 `q_hash`
+ 48 `best_lru` + ~45 scan/decision state + 9 scanner response + 13 adapter
response ≈ **330**, plus 80 for `rd_q` *if* Quartus does not absorb it into the
M10K output register (it is unconditional and unreset, the absorbable form) →
**330–410 vs 1,490**. Combinational: one 32-bit equality, one 48-bit magnitude
compare, one 48-bit incrementer, the four 32-bit counters the old block also has,
a 16:1 valid-bit select and a 16-way valid-bit set, FSM — against sixteen 32-bit
equalities, a sixteen-way 48-bit minimum tree and an 80-bit-wide 16:1 select.
Memory: **2 M10K** (or 4 MLAB). I decline to convert that into an ALM number; a
counted netlist is not a mapped one, and the gate below exists to replace this
paragraph with a measurement.

**UNKNOWN:** the candidate's ALM count, Fmax, whether inference lands as
predicted, whether `rd_q` is absorbed; the production acquire demand (no
consumer, no trace); whether 20 clocks per acquire-once-per-association is
acceptable to a consumer that does not exist yet (it is ~0.2 % of the EARTH
allowance per association, which reads as fine, and is nonetheless a prediction
about a consumer nobody has written).

## 9. The one fit gate — named, not run

**F-PROGDIR1** (`design/fit_targets.yml`, target `zhao_field_progdir` = scanner +
adapter). Question, in §5's words: *"map the scanner plus adapter: price the
memory and comparator, not only the unwrapped leaf."* Concretely:

1. Did the 16×80 directory become block memory? (`min_m10k: 2` — the MINIMUM is
   the tripwire; rows falling back to flops would be the old block with a slower
   interface and Fmax would call it a pass.)
2. Registers ≤ 700 (prediction 330–410; the old block's 1,490 must fail).
3. What do one hash comparator + one stamp comparator + scan control + the
   adapter's channels cost, against 2,237 est. ALM?

Everything else — correctness, order, ties, wrap, interval — is already answered
by Verilator and is not a reason to spend a fit. Per the batching rule this gate
should ride with the next FIELD-domain fit rather than run alone; the block is
outside every currently live lane's closure (common/geometry/forge), so it can
wait without blocking anyone.

## 10. Not verified — by item, with the instrument that would do it

* **ALM / Fmax / inference of the candidate** — F-PROGDIR1 (Quartus). Not run.
* **Absorption of `rd_q` into the M10K output register** — same map; read the
  "Inferred RAM" lines and the register count.
* **Synthesizability under Quartus 17.0.2** — `check_quartus17_syntax.py` is
  clean, but *"a block that has never been through quartus_map has not been
  shown to be synthesizable"*. This one has not. The `unique case`, the
  `IDXW'(...)` casts and `(* ramstyle *)` are all forms already in mapped
  blocks in this tree, which is comfort, not evidence.
* **A tie fault caught at LRUW=48** — unreachable with legal stimulus by
  construction (2^48 stamps); demonstrated at LRUW=2 with the same comparator
  and the same RTL. The mutant control does not run at 48.
* **Cycle-level interleavings the harness cannot produce** (see §3's limits
  paragraph) — need a real consumer; the same missing consumer that makes the
  acquire demand UNKNOWN.
* **`programs_rejected_o` saturation at 0xFFFF_FFFF** — the saturating branch
  is the oracle's line copied; not driven to the limit (2^32 rejections).
  Reachable only by a `--public-flat-rw` deposit; not done.
* **The CMake registrations compile in the shared tree** — I built everything
  standalone (`verilator_bin.exe` per the env script; `VM_PARALLEL_BUILDS=1`
  because the `__ALL.cpp` rule wants a `python3` that is not on PATH; absolute
  include paths; `-std=gnu++17`). The `tests/CMakeLists.txt` additions mirror
  the existing `field_progcache` block and the `proj_matw_mutant` wrapper
  pattern but have NOT been run through `cmake --preset` — two other lanes are
  building in the shared tree and the build note's regeneration race is real.
  Note the re-pointed suite needs `zhao_zref` (for `zfield_decode`) and
  `runtime/include` (for `zhao_abi.h`), which `zhao_zref`'s PUBLIC include dirs
  provide in CMake; the standalone build had to add it by hand.

## 11. Claims checked, and the ones refused

Verified true: the old block keeps hashes and stamps in fabric and compares in
parallel (RTL); 2,237/1,490 is map-only and describes today's file (JSON +
`git diff`); the scoreboard's "0 fit, 1 map, 12 UNPRICED" for FIELD (tool
output); 16×(1+32+48) ≈ 1,300 flops (arithmetic); 16×80 → 2 M10K (port-width
arithmetic, §7); `zhao_geom_quat2mat_mutant.sv` is the current pattern (exists);
the build note's `-std=gnu++17` / absolute-paths / no-spaces rules (all three
bit me or would have).

**Refused — the twelfth, and it is in the contract this packet replaces:**
FIELD.PROGCACHE.md and the old block's header say *"Scanning it would cost
sixteen cycles to save almost nothing."* The map row says "almost nothing" is
2,237 estimated ALMs and 1,490 registers — roughly 5 % of the device for a
sixteen-entry directory. The sentence was an argument that stood in for a
measurement, was cited in two places, and settled the shape of the block for
twenty days. It is withdrawn in the contract amendment.

**Refused — mine:** the first version of the tie script computed *zero* touches
at ENTRIES=2 LRUW=2, built no tie, and the positive control reported "mutant
agrees on everything" — a checker that could not see a tie fault, reading as
a passing control. Caught only because the control's polarity is inverted: a
control that finds nothing is a FAIL, and it failed. The arithmetic (k must be a
full wrap when the modulus divides E−2) is fixed and the comment beside it says
what happened. Also mine: my header derived the response at edge N+2; the
measured 20 is N+4 — the adapter's channel register and the harness's "takeable"
convention each add one. The header now quotes the measured number and states
the convention.

**Two stale-instrument notes found in passing (not mine, not touched):**
`reports/synthesis/RAM-INFERENCE-SCAN.txt` still carries the pre-2026-09-04
strong wording ("an M10K has no reset port, so this array cannot be one") that
`check_ram_inference.py`'s header has since downgraded to a weak signal — the
scan file predates its own tool's correction. And the manifest check fails
today on `zhao_forge_cliff_ram`, the FORGE lane's new module; not this lane's to
register.

**Where the brief was wrong, plainly:** nowhere material. Two things it
inherited were imprecise: "~35 clocks" describes a different candidate shape
than the one the house idiom yields (20 measured, §4), and "the adapter records
the request kind until its response is accepted" is done here by the scanner
echoing the kind rather than by adapter-side state — same law, one register
instead of two.

## 12. Reproduce (standalone, no CMake, from Git Bash)

```
# env per tools/env/zhao-env.ps1: VERILATOR_ROOT=.tools/oss-cad-suite/share/verilator,
# PATH += oss-cad-suite/bin, oss-cad-suite/lib, C:/programmieren/dsstuff/mingw64/bin
verilator_bin.exe --cc --exe -Wall --top-module tb_field_progdir_diff -GENTRIES=16 -GLRUW=48 \
  --Mdir build-progdir/diff16 -o diff16.exe \
  -CFLAGS "-std=gnu++17 -O1 -DTB_ENTRIES=16 -DTB_LRUW=48 -I<abs>/tests/harness -I<abs>/tests/field" \
  tests/field/tb_field_progdir_diff.sv fpga/rtl/field/zhao_field_progcache.sv \
  fpga/rtl/field/zhao_field_progdir.sv fpga/rtl/field/zhao_field_progdir_scan.sv \
  tests/field/field_progdir_differential.cpp tests/harness/zhao_sim.cpp
mingw32-make -C build-progdir/diff16 -f Vtb_field_progdir_diff.mk VM_PARALLEL_BUILDS=1 -j6
./build-progdir/diff16/diff16.exe            # directed + timing probe
./build-progdir/diff16/diff16.exe --random 200
# ties: -GENTRIES=2 -GLRUW=2 (-DTB_ENTRIES=2 -DTB_LRUW=2); control: add -DPROGDIR_MUTANT and
# replace the two progdir .sv with tests/mutants/zhao_field_progdir_scan_mutant.sv, main =
# tests/field/field_progdir_mutant_control.cpp
```
`build-progdir/` is under the `build-*/` ignore rule and holds nothing but
these binaries; `build.sh` there is a convenience for this session only.
