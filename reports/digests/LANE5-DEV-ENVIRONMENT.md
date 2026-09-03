# LANE 5 — THE DEVELOPMENT ENVIRONMENT

**Recon digest. Comprehension and extraction only; nothing was moved, built, fitted or committed.**
Scope: ZEMU, the parallel PC/console plan, the language's growth rule, and a triage of
`reports/REMAINING_BLOCKERS.md`.

**Dates established by `git log`, not mtime** (rebases stamped the tree with today):

| document | true commit date | commit subject |
|---|---|---|
| `SaveTheRendered.md` (repo ROOT) | **09-03 10:23** | *"Agent please read. After the islands, this is next."* |
| `reports/ZEMU_OMNISCIENT_DEVELOPMENT_MACHINE.md` | **09-03 09:44** | *"…Put this jewel where it belongs… Emulator directory or something"* |
| `reports/Islandrearchitect3.md` | 09-03 07:24 | *"Agent please read - important info"* |
| `reports/OWNER-RULINGS-BUILDABILITY-20260902.md` | 09-02 21:44 | buildability brief |
| `reports/OWNER-RULINGS-COMPLETE-20260831.md` | 08-31 22:17 | all 28 blockers answered |
| `reports/Future.md` | 08-31 07:43 | *"Agent please read - new instructions"* |
| `reports/REMAINING_BLOCKERS.md` | **08-28 07:27** | (last touched) |
| `reports/Fieldv3.md` | 08-27 12:50 | *"We have to rearchitect field again."* |
| `reports/Headache.md` | 08-27 09:09 | creature-lane brief (peer-owned) |
| `reports/FIELD_RESOURCE_MODEL.md` | 08-25 16:26 | banked-RF probe re-fit |
| `reports/EARTH60_CAPACITY.md` | 08-25 18:12 | Earth histogram |
| `reports/FIELD_IR_ENGINE.md` | 08-25 15:31 | PROGCACHE sweep |
| `reports/status/phase2_wave2.md` | 08-22 22:17 | wave-2 status |
| `reports/status/phase1_wave1.md` | 08-15 06:14 | wave-1 status |

**The single most important date fact:** `REMAINING_BLOCKERS.md` stopped on **08-28**, and
**two owner ruling sets landed after it** (08-31, 09-02). A large fraction of what that file
calls "blocked" was ruled open in the four days it did not cover. Triage below reflects that.

---

# 1. ZEMU IN ONE PAGE

**What it is.** ZEmu is *not* proposed as "a desktop program that runs Zhaozhou games."
It is proposed as **the place where the game is normally developed** — a whole-machine
executable oracle that is simultaneously a flight recorder, a deterministic time machine,
a state laboratory, a source-level profiler, a hardware differential harness and a mission
test farm. Its one-line thesis:

> Make every important fact about a running Zhaozhou universe observable, reproducible,
> addressable, comparable, and manipulable without changing its meaning.

**Where it sits in the existing hierarchy** (the treatise does not disturb this):
ZSpec says what the machine *means* → **ZRef** is the slow scalar executable law →
**ZEmu** is the complete executable world, optimised but exact → **ZRTL/Verilator** proves
the implementation → **the physical FPGA** proves clocks, bandwidth and silicon.
"Ultimate oracle" means *ultimate whole-machine development oracle* — explicitly **not**
permission for an optimised ZEmu path to redefine a scalar law. On a semantic dispute,
ZSpec and ZRef win.

**What "omniscient" means concretely.** Six mechanisms, not a mood:

1. **One canonical whole-machine state** (§6) — gameplay/HPS, local VRAM, processor and
   device state, and host-virtualised state — validated by a brutal completeness test:
   *save at a boundary, kill the process, restore in a fresh process on another host, and
   produce byte-identical future state hashes, frames, audio, counters and faults under the
   same input journal.* Anything needed to pass that is in the snapshot contract.
2. **A shared time coordinate** (§7) — sim tick, frame id, packet seq, fabric cycle,
   scanline/dot, input seq, audio sample, event id — never flattened into "frame number",
   and every coordinate labelled exact / derived / estimated / unavailable.
3. **Semantic memory** (§11) — the compiler emits a versioned reflection schema (working
   name `debug.zschema`) so RAM *explains itself*: `world.creatures[id=19].brain.mode`
   resolves to type, value, raw bytes, domain, address, source id, owning system and last
   write. Hand-maintained debugger structs are explicitly forbidden as drift-prone.
4. **Time travel** (Part III) — incremental content-addressed snapshots, an input/external-
   event journal, reverse execution by restore-and-replay, **timeline branching as a
   first-class object**, run-until *semantic* predicates, and automatic binary search for
   first divergence that distinguishes first *machine-state* from first *gameplay-visible*
   from first *pixel* divergence.
5. **Causality** (Part IV) — last-writer maps, write journals, source/event provenance,
   optional dynamic taint; plus a **provenance-honesty rule** separating recorded parentage
   from data dependency from mere temporal correlation. "Causal" must not label a guess.
6. **Component substitution** (§9) — every block selectable as optimised software / scalar
   ZRef / Verilated RTL / remote physical FPGA / deliberately mutated test implementation.
   The chosen map is part of the run manifest and the state-hash identity. This is what makes
   automatic blame-bisection possible.

**Ten non-negotiable laws** (§5), the ones that bind day-one design:
headless is primary (UI is a client); exactness is explicitly declared per mode; every
stateful influence is serializable or declared external; **raw truth always available beneath
any decoder**; semantic truth is first-class; observation must not silently alter behaviour;
failure creates evidence automatically; every agent mutation is transactional and audited;
**host speed and console cost are different measurements**; evidence outranks prose.

**Product split** (§30): `zemucore` (deterministic library, no GUI/OS-device assumptions) →
`zemu` (CLI, one-shot jobs) → `zemu serve` (session daemon) → graphical clients, which get
**no secret operations** — every UI action must be expressible through the public protocol.
First transport is newline-delimited JSON over stdin/stdout. Exit codes are enumerated so
**CI and agents never scrape English** to learn whether a run succeeded.

**Artifact families** (§73) — the treatise explicitly refuses to overload `.zcap`:

| artifact | purpose |
|---|---|
| `.zcap` | compact frame/capture evidence — **existing contract, keep stable** |
| `.zstate` | complete restorable whole-machine snapshot |
| `.zinput` | canonical external input/event journal |
| `.ztrace` | bounded typed trace stream |
| `.zrun` | deterministic execution manifest + result summary |
| `.zcase` | portable forensic bundle ("slurp pack") |
| `.zschema` | compiler-emitted typed state/debug reflection schema |

**What it would let the team do that they cannot do today.** Concretely:
explain one pixel back through 13 stages to the Form declaration that caused it; measure
ground penetration from *posed 3D vertices* rather than a rendered frame (the exact probe
CLAUDE.md demands and records as thrown away); replay both camera variants over the identical
battle instead of remembering which "felt better"; localise an FPGA disagreement that only
appears after twenty minutes to a narrowed cycle range and bank it as a seconds-long
regression; run a thousand deterministic trajectory branches to map reachable landing regions;
and hand an agent a `.zcase` it can *interrogate further* rather than a static crash dump.

**Requirements it imposes on the rest of the project.** ZEmu is not free-standing —
it needs the compiler to emit `.zschema`, stable identities everywhere (§12), per-device
versioned state serializers, and run manifests in machine-readable form. Several of these are
compiler and ledger work, not emulator work.

**Implementation path** — eight phases, E0–E7: E0 ratify the laws (deliverable: *a compact
ratified spec extracted from the treatise, explicitly **not** the whole essay made law*);
E1 turn the stub into a deterministic headless console; E2 semantic state and query;
E3 time travel and branching; E4 domain observatories; E5 Verilator and board lockstep;
E6 agent laboratory and farm; E7 development-provenance integration.

**The treatise's own governing risk, §103,** deserves to be quoted into any adoption:

> **Overbuilding observability before the game.** Build observability in response to real
> active lanes; keep the headless/state foundations early; require every advanced lens to
> retire a demonstrated debugging cost.

And §104: automated likeness metrics must not override the owner's eye — the treatise
explicitly re-states and preserves the project's art law.

**Status honesty.** The document brands itself *"Proposal and design treatise; not a ratified
specification"* and states that **no binary format, command name, file extension or API method
in it is frozen merely by appearing there.** It classifies every recommendation as
EXISTS / FOUNDATIONAL / RECOMMENDED / ADVANCED / EXPERIMENTAL. Treat it accordingly.

## 1a. What already exists vs. what is genuinely new

**Already exists (do not rebuild):**

| footing | where |
|---|---|
| `emulator/` product target, `zemu` binary, CMake target | `emulator/CMakeLists.txt`, `emulator/zemu_main.cpp` (85 lines, documented-EMPTY stub) |
| A **written, ledgered ZEmu contract** with wave-3 scope | `design/contracts/SW.ZEMU.md` (ledger ZH-078, phase 1, maturity SPECIFIED) |
| ZRef exact scalar oracle | `reference/`, `zref::`, `zhao::ZhaoZrefShell` |
| Sealed, CRC-protected, versioned, source-ID-bearing frame packets | `spec/capture_format.md` §3 |
| `.zcap` with resource pages, pad snapshots, FB/tile CRCs, counters, source maps, **first-divergence record** | `spec/capture_format.md` §4 |
| Canonical atomic `PadFrame` sampled at the frame boundary | `spec/input_rules.md` |
| Stable append-only counter IDs latched at frame boundaries | `spec/counters.md` |
| Deterministic HPS harness, memory/ring ownership, modeled profiles | `spec/memory_rules.md`, Verilator shell |
| The D5 **sim-hash chain `H_t`** and run-twice-identical e2e tests | `design/contracts/SW.ZEMU.md`, `tests/e2e/` |
| Cartridge container law (`.zpak`, magic ZPAK, per-section CRC-32C, CODE_MANIFEST) | `spec/cartridge.md` |
| Form frontend: lexer / parser / checker / AST + `.form` corpus | `compiler/src/frontend/`, `compiler/tests/frontend/corpus/positive/` |
| Golden captures + saved failing vectors as committed evidence | `captures/golden/`, `captures/failures/` |
| Reel tool, contact sheets, orthographic diagnostic cameras | `tools/reel/` *(peer-owned — do not touch)* |

**Genuinely new in the treatise** (nothing in the repo does these today):
whole-machine state serialization; snapshot/restore/rewind; timeline branching;
typed semantic query over compiler-emitted schema; a stable headless agent protocol;
last-writer/causal provenance; automatic forensic bundles; failure minimization;
the simulation farm; and the runtime↔development provenance graph.
The stub's own §3.2 list in the treatise agrees, and calls the absence **good news** —
these are easiest to make foundational *before* the emulator accumulates hidden host state.

**Overlap warning.** ZEmu's "differential bisection by component substitution" and
"ZRef vs Verilated block" comparison substantially overlap the **existing** Verilator
differential harness and the `.zcap` first-divergence record. The new part is *composition*
(whole-machine checkpoints, automatic bisection across a component map), not the pairwise
compare, which already works. Budget accordingly.

---

# 2. WHERE IT BELONGS

## Recommended path

```
emulator/ZEMU_OMNISCIENT_DEVELOPMENT_MACHINE.md
```

**Evidence for that exact path:**

1. **The emulator directory the owner half-remembered does exist.** Root `README.md` line 31
   registers it: `emulator/   ZEmu (desktop console shell)`. It is one of the repo's declared
   product directories, alongside `reference/`, `compiler/`, `runtime/`, `fpga/`.
2. **It currently holds two files** — `CMakeLists.txt` and `zemu_main.cpp`. A design treatise
   dropped at its top level is physically impossible to miss, which is the owner's stated
   requirement (*"make sure it never gets forgotten"*).
3. **CLAUDE.md's own durability law points here.** The section *"Instructions are not delivered
   until they are read"* rules that **a run folder is the wrong home for anything durable** and
   that **durable direction belongs beside the thing it governs.** `reports/` is the same
   failure mode one step slower: it is a 90-file undifferentiated pile in which this document
   is currently indistinguishable from `fit-round12c.log`-era ephemera. The thing this treatise
   governs is `emulator/`.
4. **A sibling-repo home is wrong.** `nanquan/` owns the language; `Upheaval/` owns the game.
   ZEmu is console tooling and belongs in `zhaozhou`, per CLAUDE.md's three-repo split.
5. **`design/contracts/SW.ZEMU.md` is not the home either** — that file is the *ledgered
   contract* (ZH-078, generated section structure, wave-3 scope). A 113 KB unratified treatise
   inside the contract tree would read as ratified law, which the document itself forbids.

## Two supporting edits to recommend alongside the move

- **`design/contracts/SW.ZEMU.md` → Notes:** add one line —
  *"Design treatise (proposal, not ratified): `emulator/ZEMU_OMNISCIENT_DEVELOPMENT_MACHINE.md`."*
  This is the pointer that makes the ledger find it.
- **Root `README.md` line 31:** annotate to
  `emulator/  ZEmu (desktop console shell) — design treatise in emulator/`.

**Acceptable alternative** if a docs subdirectory is preferred for tidiness:
`emulator/docs/ZEMU_OMNISCIENT_DEVELOPMENT_MACHINE.md`. Weaker on point 2 (a subdirectory is
one more click of forgetting), identical on every other count.

**Not recommended:** `docs/` at root — it holds cross-cutting material (`BUILD.md`,
`OWNER_DOCKET.md`, `naming.md`), and filing ZEmu there separates the treatise from the code
it governs, which is precisely the failure CLAUDE.md legislated against.

**Nothing has been moved.** This is a recommendation awaiting the owner's word.

---

# 3. THE PARALLEL PC/CONSOLE PARITY MECHANISM

`Future.md` asks for PC and console developed in parallel, **not diverging**, with the PC
version carrying online multiplayer, experimental higher resolution, an authentic mode, and
"all the PC trappings."

## The mechanism already exists in law and is only half-built in code

**The concrete artefact is not one thing; it is a four-part contract that already binds both
sides.** Named, in order of load-bearing:

### (a) The truth/form split — the mechanism that makes divergence *type-checkable*

`FORM_LANGUAGE_HARDWARE_CODESIGN.md` §1 is the whole answer to "how do the versions not
diverge while the PC has more features":

> **Truth** — terrain height and velocity, persistent scars, collision, navigation costs,
> creature state, damaging projectiles, spell timing, victory conditions, random outcomes,
> controller input snapshots. *Truth is never degraded because the renderer is busy.*
>
> **Form** — tessellation, creature representation level, polygon count, procedural
> subdivision, particle count, texture detail, glow/distortion/post, distant microforms,
> which effects are omitted under pressure. *Form may change coherently to honour 60 Hz.*
>
> **The language and type system prevent presentation code from mutating truth.**

**Therefore the parity rule writes itself, and it is a compiler-enforced rule rather than a
convention:** *every PC-exclusive feature must live entirely in the `present` domain (form),
or it is a fork.* Higher resolution, extra particles, better textures, post effects,
ultrawide — all form, all legal, all divergence-free by construction. Frozen decision 6
("truth and presentation are separate domains") and 7 ("presentation blocks are pure and
reorderable") are what make this enforceable rather than aspirational.

**Online multiplayer is the one PC feature that does NOT fit this**, and it is the single
biggest open architectural question in `Future.md` — see §3d and §7 below.

### (b) The sealed frame packet + semantic command ABI — the wire both sides speak

Charter §3.5: *"Form targets a generated semantic command ABI. It does not know whether a
command is currently lowered by ARM software or FPGA hardware."*
Charter §3.3: *"Both consume the same frame packets and asset formats intended for hardware."*
Charter §2069: *"ZRef, ZEmu, Verilator and physical hardware consume the same frame/capture
formats."* Packets are **immutable after sealing, dual-CRC, versioned and source-ID-bearing**
(`spec/capture_format.md` §3). This is the ABI that makes "the same game" mean something
byte-checkable rather than rhetorical.

### (c) The conformance suite — the gate that catches divergence

Already written into `design/contracts/SW.ZEMU.md`:

- `zemu --cart wound_lab.zpak --ticks 600` → **run-twice-identical hash chain + frame CRCs**;
- ZEmu capture **replays through `tools/capture` identically**;
- `--realtime` vs free-run **byte-identity** (*"a paced run and a free run are byte-identical;
  tested"*) — this is exactly the law that stops a PC frame-pacing feature leaking into truth;
- pack→load round-trip byte-stable; corrupt-cartridge refusal cases;
- nightly randomized soak whose **invariant is cross-run identity**, with *"the ARM cross
  target as the differential partner (W3.8)."*

**That ARM-cross-target differential is the literal PC-vs-console parity test**, and it is
already specified. Charter Phase-3 gate states the objective in one line:

> **"same Form source runs on desktop ZEmu and SuperStation HPS"**

### (d) The `runtime/` split — the code artefact, currently scaffolding only

```
runtime/desktop/desktop_main.cpp   64 lines   documented-EMPTY stub, links ZRef shell
runtime/mister/.gitkeep            0 bytes    EMPTY
runtime/include/zhao_abi.h                    the shared ABI header
```

So the *shape* is right — one shared ABI header, two thin platform mains — and the *content*
is not built. This is where the PC/console lane actually is today.

## Named parity mechanism, stated for ratification

> **Shared truth core + semantic command ABI, with every platform difference confined to the
> `present` domain, gated by a cross-target conformance suite whose invariant is
> sim-hash-chain and frame-CRC identity.**

Four gates make it real, and three of them already have contract text:

| gate | status |
|---|---|
| G1 — PC and console builds produce identical `H_t` sim-hash chains over the same `.zpak` + pad journal | **specified** (SW.ZEMU directed tests); needs a real cartridge |
| G2 — `--authentic` PC mode produces frame CRCs byte-identical to the console's | **needs writing**; the mode enum already supports it (`spec/video_rules.md` §1: Z60 / Storm / Duo, mode latched only at frame start, plus the ruled `VIDEO_WIDE` 384×216 / `WIDE_DUO`) |
| G3 — PC-exclusive features fail to compile if they touch truth | **already the compiler's job**; needs a *negative* test in the corpus proving it |
| G4 — higher-resolution PC mode changes no gameplay hash | **needs writing**; is a metamorphic test in ZEmu's §60 sense |

**The video-mode law is a gift here and should be quoted at the resolution question:** the
canvas allocation is fixed at the size of the LARGEST canvas so a mode switch never moves or
resizes a slot, and the mode latches only at frame start. Extra PC resolutions extend this
table; they do not fork the pipeline.

## The unsolved half: online multiplayer

Multiplayer is not in the truth/form split and cannot be pushed into `present`. It introduces
**a second source of non-deterministic external input**, which is exactly the class of state
that Charter, `input_rules.md` and ZEmu §5.3/§15 all legislate about. The compatible design —
and the one the existing architecture is already shaped for — is:

> **Remote pads are journal entries.** A remote player contributes `PadFrame` snapshots into
> the same canonical atomic input journal a local pad does. Truth stays lockstep-deterministic;
> the network layer is a `present`-side/host-service concern that *delivers* pad frames and
> never mutates truth.

This makes multiplayer replayable, capture-compatible, and console-identical by construction:
a replayed 4-player match is indistinguishable from a local one, and ZEmu's `.zinput` journal
is the same artefact. **It also means rollback netcode is off the table without a ruling**,
because rollback requires speculative execution and re-simulation — which ZEmu's snapshot/
restore machinery could actually provide, but which is an architecture decision, not an
implementation detail. **Flagged for the owner.**

---

# 4. `SaveTheRendered.md` — what it asks, and its island dependency

**Read the title as "Save the Renderer."** It is a 417-line technical campaign, newest
document in the repo (09-03 10:23), and it has nothing to do with saving rendered images.

**The ask:** close the existing reduced renderer at **105 MHz** without changing pixels,
ordering, throughput contracts, or hiding paths. Current best is **99.50 MHz**.

**Its central finding is a methodology error, not a wall:**

> The committed SDC says `create_clock -name gpu_clk -period 10.000`. **The fitter therefore
> optimised a 100 MHz problem.** 99.50 MHz is merely the derived Fmax of a 100 MHz placement.
> *"The current 99.50 number is the wrong experiment."*

At the true 105 MHz period (9.524 ns) the owner set becomes finite rather than terrifying:

| owner | slack @ 10 ns | deficit @ 105 MHz |
|---|---:|---:|
| Early-Z | −0.050 ns | **−0.526 ns** |
| CMD DMA | +0.042 ns | −0.434 ns |
| Binner | +0.221 ns | −0.255 ns |
| TileStore | +0.281 ns | −0.195 ns |
| EdgeWalk | +0.314 ns | −0.162 ns |

**The campaign, C0–C7,** in the document's own order:
C0 fit provenance (read back actual staged period/seed/settings; **fail the run if requested
≠ reported**; preserve 1,000–2,000 full setup paths); C1 unmodified 105 MHz baseline at
9.52381 ns, seeds 1–5; C2 TilePipe **registered coverage cursor** (move the 16-way priority
encoder off `frag_addr`'s hot path onto the already-free `cov_acc` staging edge — no bubble,
no ordering change, one fragment/clock); C3 Early-Z **active-row presence** (store the 256-bit
accumulator as 16×16 rows, prefetch the active row, replace a 256:1 dynamic lookup with
`OR(active_seen_row AND cur_hot)` — **preserving the same-edge `round_done` and floor
promotion**, which is what the earlier failed attempt broke); C4 CMD DMA header-predicate
split (parallel predicates → one-bit registers, then an 11-bit priority decision; one cycle
per *packet*, not per fragment; **exact first-error priority frozen**, with a mutation test
proving a swap is caught); C5 TileStore **only if named**; C6 Binner **only if named** (and
explicitly *do not add a serial bubble* — Binner already carries a 2.83-clocks-per-reference
throughput debt); C7 tool matrix, one variable at a time.

**Explicit do-not-retry list, already measured dead:** `OPTIMIZATION_TECHNIQUE=SPEED`
(−3.01 MHz, +147 ALMs) and explicit physical register duplication (zero change, identical
ALM count and slack). *"The repo has already killed those ideas properly."* Also:
**leave EdgeWalk alone initially** — four structural fixes already landed there and a generic
skid made it worse; it may pass once C2–C4 move the placement.

**Two apparatus bugs to fix before any of it:** (1) seed provenance is mislabeled —
`perf-seed3` contains evidence JSON and a manifest **claiming seed 1**; the collector reads the
committed default rather than the fitted assignment. (2) the per-seed bundles kept summaries
but **not the literal setup paths**, which is precisely why Binner can be named but not
responsibly redesigned.

**Stated odds** (engineering judgement, explicitly not a fabricated Quartus result):
≥1 genuine 105 MHz passing placement **80–90%** after C2–C4 + target-aware fitting;
3 of 5 ordinary seeds **60–75%**; current netlist via tool settings alone **35–50%**;
110 MHz "still uncertain and should not be smuggled into the 105 claim."

**The island dependency, precisely.** The commit message is *"After the islands, this is
next"* and the document names the coupling twice:

- **Methodology inherited from the island brief:** *"The island brief already established the
  correct methodology: fit named target periods — 9.524 ns for 105 MHz — and judge zero WNS
  and zero TNS, rather than treating a derived Fmax from another target as signoff."*
  C0/C1 are that method applied to the renderer.
- **Apparatus inherited:** the campaign extends `run_shell_fit.ps1` with a `-GpuPeriodNs`
  argument and stages the matching SDC into the clean snapshot — i.e. it needs the island
  fit harness to exist and be trustworthy first. **The seed-provenance bug (C0) is an island-
  apparatus bug**, and shipping it forward would make every renderer number unprovable the
  same way.

**Its own stated boundary, which must not be over-claimed:** success proves *the reduced
renderer at 105 MHz on the provisional virtual-pin Cyclone V fit.* It does **not** pre-prove
the texture-survivor composition, physical board I/O, SDRAM timing, or the final full machine.

**Sequencing note:** a production-only resource fit is RUNNING. This campaign is Quartus-heavy
by nature and must not be started against a busy toolchain, nor by this lane.

---

# 5. THE LANGUAGE'S DEMAND-DRIVEN GROWTH RULE

`Future.md`, verbatim:

> *"We also need the programming language. Past the normal basics, it should only grow when
> developing the game, but when a feature is needed, it gets implemented in the language as a
> first class feature."*

## This is compatible with "hardware first; stop compiler overengineering" — and it is the
## enforcement mechanism, not an exception to it

The standing memory forbids a *general compiler programme*. `Future.md` forbids the same
thing from the other side: growth is **pulled by the game**, never pushed by the compiler.
The two agree. What `Future.md` adds is a positive obligation — when the game *does* need
something, the answer is a **first-class language feature**, not a C++ escape hatch that
quietly becomes the real language.

## The bounding rules already written down

`FORM_LANGUAGE_HARDWARE_CODESIGN.md` §20 is an explicit non-goals list that pre-bounds this:
do not turn Form into a general-purpose language replacement, a dynamic OO language, a GC'd
console VM, arbitrary game-to-Verilog synthesis, a general fragment-shader language,
**a macro system before the core language works**, an LLVM research project before the C++
backend ships, or *"an excuse to delay the exact triangle pipeline."*
§21 freezes 18 decisions (static typing, AOT, fixed-tick determinism, no GC frame-loop heap,
truth/presentation domains, fixed-point and projected-pixel as first-class types, bounded
pools visible in source, **tests/captures/budgets/source-mapping as compiler outputs**,
Wound Lab as the permanent integration test).

## Proposed decision procedure — four gates per feature

Assembled from the existing laws; recommended for ratification as-is:

1. **Demand gate.** A named, committed piece of *game* work needs it. Cite the file. No
   speculative features; a hypothetical need is not a need.
2. **Escape-hatch gate.** Would C/C++ FFI do the job? §20 permits FFI for platform code and
   emergency escape hatches — but *"escape-hatch code cannot bypass capture, ownership and
   determinism rules silently."* **If the feature must be capture-visible, ownership-checked
   or determinism-bound, FFI is not an answer and the feature is genuinely first-class.**
   That is the actual dividing line.
3. **Domain gate.** Which domain does it belong to — `sim` (truth), `present` (form), `test`,
   or `build`? A feature that cannot name its domain is not designed yet. A feature that wants
   to straddle truth and form is a design error, not a language request.
4. **Cost gate.** §14 makes performance part of the type-and-build system and frozen decision
   17 makes cost reports a compiler product. A feature that hides its resource cost violates
   §20's last non-goal.

**Plus a fifth, from this recon:** does it need a `.zschema` entry so ZEmu can decode it?
Any new state-bearing construct that ZEmu cannot inspect is a blind spot the day it ships.

## Where the growth must land — and a live fork that must be ruled on first

`nanquan/README.md` states the governance plainly:

> *"Future language and compiler work belongs here, not in the Zhaozhou hardware repository."*
> *"The imported implementation still uses the legacy internal name `Form` in source syntax,
> class names, generated filenames, diagnostics, and golden artifacts. Those identifiers are
> explicit technical debt; this bounded import does not attempt a broad rename."*

**But `zhaozhou/compiler/` is still present, still live (216 workspace tests in the wave-2
status), and has ALREADY DIVERGED from `nanquan/src/`.** Verified by directory diff — the two
trees differ in `frontend/ast.ts`, `checker.ts`, `diagnostics.ts`, `index.ts`, `lexer.ts`,
`parser.ts`, `span.ts`, `backends/cpp/emitter.ts`, `field_ir/i64.ts`, `field_ir/impact_wave.ts`,
`field_ir/wave_pool.ts`, and four `generated/` files; `compiler/src/field_ir/gen_optable.ts`
and `generated/depth.ts` exist only in zhaozhou.

**This is a real, present fork of the compiler, and it directly blocks the `Future.md`
instruction:** "the language grows when the game needs it" is unanswerable while there are two
languages. **Recommend the owner rule which tree is authoritative before any feature lands.**
The pragmatic call, stated so it is cheap to reverse: *zhaozhou/compiler stays authoritative
until the console's wave-3 cartridge path is closed, because the hardware lane consumes its
generated artifacts daily; nanquan then re-imports once, rather than being drip-merged.*

## The FIRST features, driven by the game's actual needs

Ordered by what the repo shows the game actually blocked on, not by language elegance.
Note §18's L-ladder puts L1 (modules, structs, enums, functions, pools, fixed-point,
`sim`/`present`/`test` domains, controller input, C++17 out) as done-or-nearly — the frontend
exists with a positive corpus covering `declarations`, `fields`, `library`, `main`,
`presentation`, `scenario`, `systems`.

| # | feature | driven by | domain | evidence of demand |
|---|---|---|---|---|
| 1 | **Cartridge/`.zpak` emission end-to-end** — a real game cartridge the compiler produces | ZEmu cannot be a dev environment without one; **no `.zpak` exists in the tree** | build | `SW.ZEMU.md` wave-3 scope requires `form_game.hpp` + `.zpak`; `spec/cartridge.md` is written and unexercised |
| 2 | **`test` domain scenario emission** (§58) — deterministic scenarios compiling to input schedules, invariants, expected hashes, budget assertions, and a headless job manifest | this is what makes headless agent work *native rather than bolted on*, and it is the cheapest large win | test | frozen decision 17; ZEmu §58; the whole E1/E2 gate depends on it |
| 3 | **`debug.zschema` reflection emission** (§11) | ZEmu E2 is impossible without it, and §11 explicitly forbids hand-maintained debugger structs | build | ZEmu §11, §99 |
| 4 | **Creature/animation declarations** — clips, event tags, attachment sockets, representation ladders | the creature lane is the most active lane in the repo and currently authors in C++ headers | sim + present | `spec/creature_rules.md`; the peer lane's `zixxtrixx.h` is hand-written C++ |
| 5 | **Terrain field authoring at source level** — Earth programs as Form, not hand-built Field IR | `impact_wave` / `wave_pool` / `crater_ring` are TypeScript builders today | sim | `compiler/src/field_ir/*.ts` |
| 6 | **Declared ground-penetration** (`authored, never accidental`) as a language construct | CLAUDE.md makes this a hard law and it currently lives nowhere checkable | sim | CLAUDE.md "Ground contact"; ZEmu §43 wants *"authored declaration versus measured 3D penetration"* |

**Explicitly NOT first:** language server, formatter, macro system, LLVM backend, optimiser
work. §18 puts LSP/formatter at L5 and §20 bans the macro system outright until the core works.

---

# 6. STILL-REAL BLOCKERS

Triaged against everything committed **after** `REMAINING_BLOCKERS.md` closed on 08-28 —
principally `OWNER-RULINGS-COMPLETE-20260831.md` (08-31) and
`OWNER-RULINGS-BUILDABILITY-20260902.md` (09-02), which between them answered 28 questions and
re-drew the buildability ledger. **Anything below marked FIXED or SUPERSEDED carries its
evidence.** Verified-in-tree entries were checked directly during this recon.

## 6a. STILL REAL — 21 items

| # | blocker | class | evidence it is still true |
|---|---|---|---|
| S1 | **FIELD v3 EXECUTOR is not a production block.** It exists only as `fpga/rtl/synth/zhao_probe_v3_exec.sv` — a synthesis probe. `fpga/rtl/field/` has 14 `zhao_field_v3_*.sv` files and **no executor among them**. | hardware | **verified in tree this session**: `ls fpga/rtl/field \| grep v3` → dispatch, len, mulbank, noise, normalize, rf, ring, ring_svc, rot, sbank, spline, svcpath, trig, wbarb. No `exec`. |
| S2 | **`zhao_field_v3_svcpath.sv` is a test harness, not the production path** — it instantiates a probe and carries a deliberate rival agent on the multiplier bank. | hardware | stated in the lane brief; consistent with the file living beside probes and with the 08-28 contention testing |
| S3 | **Nine remaining Field ops blocked on shared-resource arbitration**, not on a sequencer. `zhao_field_exec_shared.sv` holds five shared resources (one multiplier, one isqrt, one sine, one reciprocal, one rcp24 ROM). CURVE/DCURVE/SPLINE/NOISE2/RIDGE need only the bank arbiter; ROT2/ROT3 need the shared sine; RING needs the reciprocal; NORMALIZE2/3 need the isqrt **and** the rcp24 seed ROM. | hardware | `REMAINING_BLOCKERS.md` 08-28 (newest section); detail in `reports/FIELD_V3_SERVICE_ATTACH.md` |
| S4 | **The banked-RF number is a FLOOR, not a measurement of the shipped part.** 372 ALM / 12 M10K / 93.14 MHz is a fit of `zhao_probe_banked_rf`, which addresses every bank with the SAME ROW and cannot read a register group crossing a multiple of four; **its own header says it implements no Field semantics.** The functional `zhao_field_v3_rf.sv` does per-bank address arithmetic the probe does not. | measurement hygiene | 08-28 §"One measurement that is NOT what it looks like". **Must not be quoted anywhere until the functional file has its own fit.** |
| S5 | **`zhao_probe_curve_svc` fit numbers predate the `mul_ready_i` port** and need redoing. | measurement | stated at the close of the 08-28 correction entry |
| S6 | **`GEOM.PARAMBUF` has no block, no contract and no ledger entry** — and the production geometry solution depends on it. The "choose a kMesh budget and grow the on-chip arena" path **contradicts the binding 08-31 ruling**; the answer is an external local-SDRAM parameter buffer. | **NEW, 09-02** | `OWNER-RULINGS-BUILDABILITY-20260902.md` §"CORRECTED BUILDABILITY LEDGER" D + R7 |
| S7 | **`SW.STREAM` is still a real stub with TODO sections, and it is on the critical path of the 8 km world.** The claim *"zero contracts are unwritten by accident"* is explicitly **FALSE**. | **NEW, 09-02** | same document, EXECUTIVE VERDICT + ledger D |
| S8 | **Renderer is at 99.50 MHz against a 105 MHz target**, and was never placed against the real constraint. | hardware | `SaveTheRendered.md`, 09-03 — newest doc in the repo |
| S9 | **Fit-apparatus seed provenance is mislabeled.** `perf-seed3` contains evidence JSON and manifest claiming **seed 1**; the collector reads the committed default, not the fitted assignment. Every per-seed label is unproven until C0 lands. | apparatus | `SaveTheRendered.md` §"The apparatus has two bugs to fix first" |
| S10 | **Literal setup paths were not archived per seed** — only summaries, manifests and owner tables. This is exactly why Binner can be named but not responsibly redesigned. | apparatus | same section |
| S11 | **Unconstrained input ports: 609 ports, 13,920 paths.** Paths that *start* at an input port are not analysed, so **−0.639 ns is a lower bound on the timing problem, not a measurement of it.** | measurement hygiene | 08-22 caveat, never retracted in any later document |
| S12 | **GEOM.WCACHE formal proof still fails.** `a_hit_implies_written` fails at k=4; a bulk async reset over an unpacked array is not expressible as a memory reset so the solver may start cells at 1. Making `valid_q` a PACKED vector closed a real modelling gap and **did not close the proof**; the CI lane stays unregistered rather than claiming otherwise. | hardware | 08-26; nothing in the 08-31/09-02 rulings touches formal |
| S13 | **`POST.GATHER`, `POST.COMPOSITE`, `TWOD.PLANE`, material combiner/TEXJOIN, `PART.UPDATE/COLLIDE/SPAWN`, terrain world layer** — blocked **until the R5/R6/R3/R9 rulings are adopted into contracts.** The rulings exist; the adoption work does not. | spec adoption | 09-02 ledger D. *This is now adoption work, not decision work.* |
| S14 | **Owner-blocked: the `material` writer-selection and `nav_cost` reduction laws** are declared in `design/contracts/FIELD.SEQ.EARTH.md` as **CHOSEN, NOT FOUND**. Reducer semantics rather than game content, but still an owner choice. | owner | restated in the **08-28** section — the newest statement of it |
| S15 | **Owner-blocked: whether 27 bits covers world coordinates.** Gates up to **110 DSPs** — recorded as *"the highest-value question outstanding."* | owner | 08-24 queue §"Still blocked on the owner"; the 8 km world rulings raise rather than settle it |
| S16 | **Owner-blocked: the scar-texture POOL SIZE.** `SURFACE.STAMP` is **pool-bound, not rate-bound**; Sacrifice's `GetFreeScarTexture`/`ReleaseScarTexture` prove a finite copy-on-write pool existed but its capacity is not recoverable. | owner | 08-23 and 08-24, both "still blocked" |
| S17 | **`zref::rescale_s32` silently narrows `__int128` → `int64_t`** in the shipped skinning reference. Not a regression and unreachable with a real bone matrix (0 of 24,000 pose-range coordinates), but **three options are docketed and none is taken.** | owner / correctness | 08-23 and 08-24 |
| S18 | **Three-bone skinning tail (2.51% of vertices) and the weight-normalisation precondition** — undecided. | owner | 08-24 |
| S19 | **`TERRAIN.BAKE` backlog route is unchosen** — neither the pipelined-divider nor the fetched-stencil option has been chosen *or measured*, and the directive requires that before terrain sign-off. | hardware | `EARTH60_CAPACITY.md` §"What this does NOT claim" |
| S20 | **The `TERRAIN.PATCH` `1 + n` intake ceiling** — the *other* Earth60 ceiling, which **no amount of Field-side work can move.** `Fieldv3.md` proposes field-major patch accumulation as the fix; it is unbuilt, and the model for it is a separate deliverable that was never written. | hardware | `FIELD_RESOURCE_MODEL.md` §"What this does not cover"; `Fieldv3.md` §"The terrain side must change too" |
| S21 | **Board-gated, not solvable in simulation:** `SYS.PLL`, `SYS.RESET`, `SYS.CDC`, `MEM.SDRAM`, `SW.TOOLS.BOARDPROBE`. The audit's *"nothing waits on hardware"* is **too broad** — PLL frequencies, reset release timing, CDC topology, SDRAM timing, sustained bandwidth and calibration are board truth. Carries the ZH-004 obligation that byte-address bit 25 selects the bank on the real device. | hardware-blocked | 09-02 ledger B; `reports/blocked_on_hardware.md`; phase2_wave2 §5 |

### Dev-environment blockers this lane adds

| # | blocker | why it is real |
|---|---|---|
| S22 | **The compiler has forked.** `zhaozhou/compiler/src` and `nanquan/src` differ across the entire frontend, the C++ emitter, three Field IR programs and four generated files. `nanquan/README.md` claims ownership of all future language work; the hardware lane consumes zhaozhou's generated artifacts daily. | **verified by directory diff this session**. Blocks `Future.md`'s language instruction outright. |
| S23 | **No `.zpak` cartridge exists anywhere in the tree.** `spec/cartridge.md` is written; nothing has been packed. ZEmu's entire wave-3 contract, and therefore "ZEMU running and playing the game", starts here. | **verified this session** — `find . -name '*.zpak'` returns nothing outside build dirs |
| S24 | **`runtime/mister/` is empty (`.gitkeep` only)** and `runtime/desktop/desktop_main.cpp` is a 64-line documented-empty stub. The PC/console parity lane has a correct shape and no content. | **verified this session** |
| S25 | **`demos/wound_lab/` contains one file** (`duo_markers.cpp`) — and Wound Lab is frozen decision 18, *"the permanent language, emulator and hardware integration test."* The permanent integration test is a marker demo. | **verified this session** |

## 6b. FIXED or SUPERSEDED — do not re-derive these

| claim in REMAINING_BLOCKERS | verdict | evidence |
|---|---|---|
| Open-loop DOT claimants on the shared multiplier — **executor half** | **FIXED 2026-08-28 06:41** | sixth attempt drew the pipeline cycle-by-cycle; whole sequence now issues from S4 where operands sit in non-moving registers. **DOT programs wrong under contention: 4–5 of 12 → 0 of 12.** 31 directed + 400 randomized green **with the DOT skip removed**. Cost stated honestly: one context 66→69 clocks, eight contexts 166→190. Commit **357a702** (whose message is about a creature — a concurrent session ran a banned `git commit -a`); record in `runs/CLAUDE-RUNS/RUN-20260827-1747-field-v3-rearchitecture/TASK_LOG.md` |
| `zhao_probe_curve_svc` had the same defect | **FIXED & VERIFIED 08-28** | `mul_ready_i` added, `F_ISSUE` holds until granted. 5,022 directed + 7,200 random green; 24 groups under refusal with 11 real refusals, answers unchanged; four-point CURVE II 13 clocks unmoved; lone-reply latency 17 cycles unmoved. Three mutants C16–C18 attack the hold. **Fit numbers still need redoing (→ S5).** |
| `zhao_probe_dist_svc` had the same defect | **NEVER EXISTED** | *"has no multiplier port at all — it takes `req_n2_*` already squared, by the service boundary its own header states."* The claim was **inherited and never checked.** A model case of a blocker that was pure documentation. |
| **CMD.DMA cannot be fitted** (needed 83,977 ALMs against 41,910) | **FIXED** | staging buffer is real M10K: **3,607 ALMs, 8.6% of the device**, placed and routed — not estimated. The `blit_buf` async-read defect named as THE composed-fit blocker no longer exists. Marked *"RESOLVED 2026-08-28. Verified against the RTL and the fit, not assumed."* |
| The composed fit needs a bigger machine (42:33, 6.2 GB) | **SUPERSEDED** | measured ~4 minutes at 5.0 GB on this machine. Both halves of the brief wrong. |
| Bit-serial CRC owns the two worst timing families | **FIXED** | `zhao_crc32c_step` (224 XOR levels) replaced by `zhao_crc32c_fold`, bit-exact against the shipped CRC, with a **`no_serial_crc` gate so it cannot return** |
| Audio Gray-decode timing family (−14.9 ns) | **GONE** | absent from all 13,651 paths of the full census — *"confirmed gone, not merely unreported"* |
| Widescreen undecided | **RULED** | `VIDEO_WIDE` 384×216 from a 384×224 tiled canvas (exact 5× to 1080p); `WIDE_DUO` 2×192×144. Prerequisite recorded: *"enum value 3 is free" is **false in practice*** — three `else-is-DUO` ternaries, a three-entry `ZHAO_TIMING` table, and a `default:` arm that would make a fourth mode silently fetch nothing |
| `zref_video.cpp` OOB read | **FIXED** | returned `kTable[mode & 3u]` from a three-entry table; fix provably golden-neutral |
| Guard range check | **DONE 08-22** | violation counter follows the registered pulse instead of the verdict: **1,383 paths → 28**, 125 failing endpoints → 97, no-escape proof still passes |
| Three Field IR pieces unswept | **DONE 08-22** | reciprocal 23/23, sine/cosine 20/20, length/distance 21/21, no survivors. Every Field IR piece now carries a mutation score |
| `TERRAIN.LOD.md` wrong about its own block | **DONE 08-22** | corrected and **measured**: 2,086 ALMs, 28 DSPs (a quarter of the device's 112). Old text said 4 comparators/0 multipliers; it is 12 and 24 — the count had stopped at the `ladder()` function instead of its four call sites |
| Five `FIELD.SEQ.*` blocks blocked on each other | **LEDGER DEFECT** | ruled one engine, five profiles: `kind: profile`, `implemented_by: FIELD.SEQ.CORE` under rule V21. What remains is lane binding — software and shell, not RTL |
| The three `SYS.*` blocks | **LEDGER DEFECT**, not a work item | 08-22 survey |
| `GEOM.MESHFETCH` cull law undefined | **DEFINED** | "visibility sectors" deleted (the phrase appeared only in the block's own purpose line). Law is conservative per-camera frustum rejection of a **bounding sphere** before vertex decode, rejecting only when outside every active camera. Its LOD third is already built (`zhao_geom_lod.sv`) |
| **"Six of the nine remaining blocks have no oracle / all six blocked on specification"** (08-26) | **LARGELY SUPERSEDED by 08-31 + 09-02** | 09-02 ledger C marks **BUILDABLE NOW**: `GEOM.PROJECT` depth carry, `GEOM.MESHFETCH`, **`GEOM.VDECODE` format 0 (RAW/CANONICAL)**, `GEOM.LOOM` (transform composition only), `FORGE.PRIM` (six v1 families), `PART.STATE` (after a qformats amendment), `PART.LADDER`, `TWOD.SPRITE`. **The specification wall the 08-26 survey found has been substantially demolished by the owner.** |
| `GEOM.VDECODE` blocked on a compression format with no owner at either end | **PARTLY SUPERSEDED** | 08-31 §5 rules *version the format, land raw first*; 09-02 makes **format 0 buildable now** and puts packed formats 1 and 2 behind a bake-off gate. The deep three-step blocker (define format → write packer → write model) now applies only to the packed formats |
| `MEASURE.HISTOGRAM` is blocked | **CLOSED AS REFUSED** | 09-02 R10 — *"MEASURE.HISTOGRAM REMAINS REFUSED"*, listed under **A. DELIBERATELY ABSENT**. Not a lane. The 08-26 finding (its `fragment_error` input resolves to a one-bit protocol flag on RASTER.FRAGMENT, not an error magnitude, and its oracle is the tenth phantom in the ledger) stands as the *reason*, and the discipline worked |
| `GEOM.WARP` needs RTL | **DELIBERATELY ABSENT** | 09-02 ledger A — ARM lowering remains valid. Also `INPUT.SNAC`, `POST.ECHO` |
| Depth quantisation is the renderer's last specified gap | **CLOSED** | 09-02 R1 — three profiles ruled, generated and proved (`WORLD_LONG` 2^40, `WORLD_STANDARD` 2^39, `CLOSE` 2^38); selection via `SetView.flags[1:0]`. *"Do not choose wmin/wmax/scale again."* |
| `276,480` has an ambiguous meaning | **CLOSED** | 09-02 R2 — conservative **PRE-Early-Z covered fragments in Z60 at 3.0× overdraw**. Not a sample count |
| Particle behaviour blocked | **SUBSTANTIALLY FROZEN** | 09-02 — remaining blocker is only the numeric interpretation of the 128-bit record (R3) |
| `TWOD.SPRITE` / `TWOD.PLANE` / compositor order undecided | **ALREADY DECIDED** | 09-02 — SPRITE is HUD-only on the primary TMU; PLANE is one restricted time-multiplexed engine; compositor order frozen |
| **"One tiny Form program parses, type-checks and lowers to deterministic C++" — PENDING** (phase1_wave1, 08-15) | **NO LONGER PENDING** | **verified in tree this session**: `compiler/src/frontend/{lexer,parser,checker,ast,diagnostics,span,exact_constant,index}.ts` all exist, with a positive corpus at `compiler/tests/frontend/corpus/positive/` covering declarations, fields, library, main, presentation, scenario, systems, plus `.form` fixtures under `compiler/tests/form/fixture/` |
| `OPTIMIZATION_TECHNIQUE=SPEED`; explicit physical register duplication | **MEASURED DEAD ENDS — do not retry** | SPEED: −3.01 MHz, +147 ALMs. Register duplication: exactly zero change, identical ALM count and worst slack |

**Count: 25 still-real (21 from the file + 4 dev-environment findings this lane adds);
25 fixed or superseded with evidence.** The 08-26 "specification wall" is the single biggest
correction — roughly a third of what that file presents as blocked was answered by the owner in
the four days after it stopped being updated.

## 6c. A methodological note the triage earned

Two of the fixed entries are *documentation* defects rather than engineering ones:
`zhao_probe_dist_svc`'s "identical defect" was **inherited and never checked against the RTL**,
and the five `FIELD.SEQ.*` "blocks" were a ledger artefact. Both cost real attention. This is
ZEmu §101 (*documentation poisoning*) happening in the repo right now, and it is the strongest
practical argument for that treatise's claim-lineage proposal (§70): status, supersedes /
superseded-by links, and evidence ranking. **`REMAINING_BLOCKERS.md` itself is the exhibit** —
2,249 lines in which the newest ruling sits 1,000 lines above an entry it silently overturns.

---

# 7. CONTRADICTIONS WITH THE STANDING HARDWARE-FIRST PRIORITY

For the owner to rule on. Stated neutrally; this lane takes no position beyond noting cost.

**C1 — `Future.md` explicitly blocks the goal on dev-environment work.**
> *"Please start implement these before you finish the goal. That way the goal will remain
> unfinished until you finish implementing these issues."*

This makes ZEmu, PC/console parity and language growth **gating on console completion**. The
standing priority is hardware-first. These are reconcilable only by reading `Future.md` as
*"do not declare the console finished while the dev environment is missing"* rather than
*"stop hardware work now."* **That reading needs the owner's confirmation**, because the other
reading redirects the entire current campaign.

**C2 — ZEmu is very large, and the treatise says so about itself.**
§103 is the author warning the reader: *"the emulator becomes another infinite infrastructure
project."* Phases E0–E7 span from "ratify laws" to "adversarial gameplay agents and mission
fuzzing." A hardware-first project should adopt **E0–E2 and stop**, treating E3+ as pulled by
demonstrated debugging cost. E0's deliverable is explicitly *a compact ratified spec extracted
from the treatise, not the entire essay made law* — that framing is the safeguard, and it
should be honoured.

**C3 — ZEmu requires compiler work that hardware-first defers.**
`.zschema` reflection emission (§11) is a *compiler* feature, and §11 forbids the alternative
(hand-maintained debugger structs) as drift-prone. So ZEmu E2 cannot happen without the
compiler moving. That is language work reaching the critical path through the emulator — worth
an explicit ruling, since it is exactly the shape the "stop compiler overengineering" memory
was written against, arriving from an unexpected direction.

**C4 — Online multiplayer has no home in the current architecture.** See §3d. Journal-based
lockstep fits the existing laws; rollback does not, without a ruling.

**C5 — Three 09-03 briefs are outstanding simultaneously** — `Islandrearchitect3.md` (07:24),
ZEMU (09:44), `SaveTheRendered.md` (10:23) — plus `Future.md` from 08-31, and a production
fit is RUNNING. `SaveTheRendered` says *"after the islands"*; `Future.md` says *before* the
goal finishes. **Nothing states the order between the ZEMU/Future lane and the
islands→renderer lane**, and they compete for the same Quartus toolchain and the same
attention. **This is the sequencing question worth asking the owner first.**

**C6 — The compiler fork (S22) is a governance contradiction already in flight**, not a
hypothetical one.

---

# 8. ACTIONABLE PLAN

Sequenced, cheap-to-reverse, and deliberately stopping short of a general programme.
Nothing here has been executed; this lane is reconnaissance.

## Immediate — zero risk, minutes

| # | action | why |
|---|---|---|
| A1 | **Move the treatise to `emulator/ZEMU_OMNISCIENT_DEVELOPMENT_MACHINE.md`**, add the `SW.ZEMU.md` Notes pointer and the root-README annotation | discharges an outstanding owner instruction from 09-03 09:44 |
| A2 | **Correct `REMAINING_BLOCKERS.md`'s superseded entries in place**, or prepend a dated supersession header pointing at the 08-31 / 09-02 rulings | the file's own §"Blockers that are GONE, and should not be re-derived" convention already exists; a third of the file now needs it |

## Near term — the E0/E1 slice, and nothing beyond it

| # | action | gate |
|---|---|---|
| B1 | **Ratify the ZEmu laws (E0)** as a compact spec — recommend `spec/emulator_rules.md` beside the other `*_rules.md` — covering only: headless-primary, exactness-declared, serializable-or-declared-external, raw-truth-available, observation-non-perturbing, transactional mutation, host-time ≠ console-cost, and the artifact separation from `.zcap` | one page, not 3,027 lines |
| B2 | **Pack the first real `.zpak`** from the Form corpus (S23) | `zemu --cart <x>.zpak --ticks 600` runs; pack→load round-trip byte-stable — **both already written into `SW.ZEMU.md`'s directed tests** |
| B3 | **E1: stub → deterministic headless console.** Cartridge load, canonical pad-journal injection, structured NDJSON responses, enumerated exit codes, whole-state hash, first snapshot/restore at a frame boundary | the SW.ZEMU wave-3 loop already specifies the tick order: *pad snapshot → sim_tick → present_frame → sealed packet → zref::render → RGB565 + CRC → optional PCM*. **The fresh-process restore test is the real gate**, per ZEmu §6 |
| B4 | **Fill `runtime/desktop` and `runtime/mister` against the shared `zhao_abi.h`** (S24) | Charter Phase-3 gate: *same Form source runs on desktop ZEmu and SuperStation HPS* |
| B5 | **Write G2/G3/G4** — the authentic-mode CRC identity test, the negative compile test proving presentation cannot touch truth, and the resolution-invariance metamorphic test | these three are the parity mechanism made executable |

**Stop there.** E3–E7 (branching, observatories, farm, provenance graph) wait for a demonstrated
debugging cost, per §103.

## Owner decisions to request, in priority order

1. **Sequencing** — ZEMU/`Future.md` lane vs. islands→`SaveTheRendered` lane (C5). Nothing
   states the order and they contend for one toolchain.
2. **Which compiler tree is authoritative** (S22) — blocks the language instruction entirely.
3. **Multiplayer model** — journal-based lockstep, or rollback (C4).
4. **Does `Future.md` gate console completion, or run beside it** (C1).
5. **27 bits for world coordinates** (S15) — still the highest-value hardware question, and it
   gates up to 110 DSPs.
6. The remaining owner-blocked set: S14, S16, S17, S18.

## Explicitly out of scope for this plan

A general compiler programme; any Quartus/synthesis/fit run; the Field v3 executor (hardware
lane); the renderer 105 MHz campaign (its own lane, after the islands); anything under
`tools/reel/`, creature/Upheaval art paths, or `active-v9` (peer-owned).

---

*Lane 5 recon. Read: `ZEMU_OMNISCIENT_DEVELOPMENT_MACHINE.md` (3,027 lines, complete),
`Future.md`, `SaveTheRendered.md`, `REMAINING_BLOCKERS.md` (2,249 lines), `Headache.md`,
`EARTH60_CAPACITY.md`, `Fieldv3.md`, `FIELD_RESOURCE_MODEL.md`, `FIELD_IR_ENGINE.md`,
`reports/status/*`, plus `SW.ZEMU.md`, the charter, `FORM_LANGUAGE_HARDWARE_CODESIGN.md`,
both owner ruling sets, and direct verification in `emulator/`, `runtime/`, `compiler/`,
`fpga/rtl/field/`, `fpga/rtl/synth/`, `demos/` and `nanquan/`.
No file was moved, deleted, built, fitted, published or committed.*
