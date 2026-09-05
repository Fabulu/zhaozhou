# Zhaozhou v1 — the release definition

**Pinned to `zixxtrixx-v8-closeout` @ `2c14cd2a`, 2026-09-05.** This is G0's first
deliverable: what v1 guarantees, which implementation supplies each capability,
and what is deliberately deferred. It is deliberately short. It is not an audit
and it does not reopen settled decisions.

**Its one job:** answer *"what are we building?"* with one row per capability, so
that a claimed pass can be checked against a named implementation instead of
against an intention.

---

## 0. The three verdicts every capability carries

A capability is not "done" until all three hold, and a block can pass any two
and fail the third:

| Verdict | Question |
|---|---|
| **Functional** | Does the intended work travel through the machine and produce the correct result? |
| **Capacity** | Does it complete the promised workload inside the frame and memory budgets? |
| **Physical** | Does the selected configuration fit, with acceptable setup/hold and resource reserve? |

Status words used below mean exactly this and nothing more:

* **SELECTED** — named in `design/prod_manifest.yml` as the one production
  implementation of its function.
* **BUILT** — RTL or software exists and has its own tests.
* **MEASURED** — has a fit row whose `sourceCommit` is not older than its source
  (see `tools/quartus/check_rule_freshness.py`).
* **COMPOSED** — proven through the shell or host, not only as a leaf.
* **ADOPTED** — SELECTED *and* COMPOSED. This is the only word that means the
  production path uses it.

---

## 1. What v1 guarantees

The target is a **feature-complete v1 console running end to end in simulation**,
a **playable desktop implementation on the same game code and assets**, and a
**complete MiSTer-targeted bitstream passing resource and timing analysis under
documented platform assumptions.**

v1 guarantees, in gameplay terms:

* Two wizards, two controllers, two views (Duo), on a persistent destructible
  island.
* Creatures that move, are lit, and are animated from resident clip banks.
* One complete rendering machine shared by desktop and console — same
  simulation, same resource and command interfaces.
* Terrain that survives traversal, deformation, eviction and return.
* Bounded spell lighting, fog, liquids, particles and a final compositor, each
  to a stated tier rather than "as much as fits".

v1 does **not** require fifty creatures, the campaign, matchmaking, or any
speculative extension. It requires that the **selected** capabilities work as one
machine and meet their promised tiers.

---

## 2. Which implementation supplies each capability

Read this with `design/prod_manifest.yml`, which is authoritative for selection.
Where they disagree, the manifest wins and this file is stale.

### Texture path — G1, the immediate priority

| Capability | Implementation | Status |
|---|---|---|
| Texture cache | `zhao_texture_cache_pipe` | SELECTED, MEASURED (1,633 ALM / 3,033 reg / 6 M10K / 101.69 MHz, 2026-09-04) |
| Sampler | `zhao_texture_tmu_pipe` | SELECTED by the manifest, **but NOT in the approved architecture at all** — see `reports/G1-ISLAND-SURVIVORS-20260905.md`. Its functions are split across five approved blocks and its palette becomes `zhao_texture_palette_res`. **Do not repair its palette.** It also has no valid fit. |
| Fragment transaction | `zhao_raster_texjoin_v2` | SELECTED, MEASURED, and known "wrong about storage" per its own ledger row |
| Fragment transaction (replacement) | `zhao_texture_fragrob` | BUILT, **not-yet-adopted**, fails its register gate at 2,631 > 2,500 |
| Material combiner (refuted) | `zhao_texture_combine` | BUILT, MEASURED, **REFUTED** — 494 ALM / 524 reg / **8 DSP** / 100.12 MHz against the architecture's rule of 2 DSP (§3.4). Docket D19q. Kept only until its replacement is measured |
| Material combiner | `zhao_texture_material_combine_v1` | BUILT, COMPOSED (`island_composed_directed`), **NOT MEASURED**. §15.5 variant A LOGIC2; all eight ratified recipes including the two three-sample terrain ones |
| Sample banking | `zhao_texture_fragrob` | FIXED 2026-09-05. It banked three sample results internally and its retire read took `res_rgb_m[0]` only, so a three-sample fragment left as sample 0 — the same fault attributed to TEXJOIN, one block upstream. Now exposes `o_s_rgb_o[3]` / `o_s_a_o[3]` |
| **Composed texture island** | `zhao_texture_island_top` | **COMPOSED** — 64 fragments in, 64 out, every block's counter moving, 0 ID errors (`island_composed_directed`, 11/11). MEASUREMENT PENDING; see `reports/G1D-COMPOSED-ISLAND-20260905.md` |

**The island as implemented is far over its own envelope.** Nine current leaf
rows of the eleven approved components sum to **8,455 ALM against a 7,500 hard
redline**, before the stale perspective-pair row and before the unbuilt material
combiner. Full table and caveats: `reports/G1-ISLAND-SURVIVORS-20260905.md`.
It is an upper bound (virtual pins, no sharing, one stale row) — and no
composed fit of the eleven exists.

**Two honest statements this table exists to prevent being blurred:**

1. **The repaired cache is not automatically the final production cache.**
   `reports/islandrearchitecture5.md` calls for a replacement cache
   architecture. A good isolated ALM result is evidence, not an architectural
   decision. Either implement the specified replacement or record explicitly
   that a revised `cache_pipe` satisfies the replacement contract.
2. **`tmu_pipe` has no valid measurement.** Its 2026-09-04 fit was stopped at
   213 minutes; synthesis evidence is preserved in
   `reports/synthesis/tmu_pipe_partial/` and shows 72,824 registers against 256
   block-memory bits (D19m). A `max_registers: 12000` gate now exists to catch
   it on any re-fit. **Do not quote a `tmu_pipe` fit number; there isn't one.**

### The rest of the machine

| Area | Status | Gate |
|---|---|---|
| Renderer shell | COMPOSED; corrected fit **99.34 MHz** — below the 105 MHz composed acceptance floor | G1 |
| Texture island | **COMPOSED 2026-09-05** and functionally proven end to end. Its CAPACITY has never been measured: every figure quoted to date is a sum of standalone per-block fits, which the census itself disproves by totalling 342 DSP against a device with 112. Composed fit in flight | G1 |
| Static geometry | **COMPOSED IN THE BENCH 2026-09-05** — all six D22 treads draw through the shell: SETUP 5 checks, DEPTHQUANT 7, CLIP 10, PROJECT 9, ASSEMBLE 8, MESHFETCH 9. The boundary now starts at a meshlet descriptor in memory. **NOT ADOPTED:** every block is in `tb_zhao_shell`, not in `zhao_shell_top`, which still instantiates one geometry block | G3 |
| Asset fetch | **NOT BUILT.** The bench plays MESHFETCH's guard, beat stream and cull. D22's remaining work is one fetcher over `GEOM.ASSET_POOL` (`memory_rules.md` §5f: 22 MiB, ENGINE1, read-only, bank 3) serving three consumers — descriptors, the u8 index stream, vertex records | G3 |
| Vertex decode | **NOT COMPOSED.** The bench holds the vertex table; GEOM.VDECODE turns 32-byte records into coordinates and step 5 moved the SELECTION, not the DECODE | G3 |
| Resource upload / residency | Generic HPS burst bridge and blit precedent exist; the general immutable-resource upload transaction is **not built** | G2 |
| Terrain | Patch/tessellation/deformation/residency machinery BUILT; world streaming (`SW.STREAM`), writeback of layer F, ordinary shading and normal detail **not built** | G2, G4 |
| Creatures | Assets and decode/skinning work exist; production pose orchestration, lighting and asset-to-renderer integration **not built** | G4 |
| Field | A composed four-wide experimental executor with exact Earth results exists; the **production** executor is not built and the manifest excludes the experimental composition | G5 |
| Particles | Numeric migration **unfinished** — codec uses six-bit world-radius scaling while `PART.EXPAND` still reads the superseded screen-pixel law | G5 |
| Post / compositor | `POST.COMPOSITE` **not built** | G6 |
| Desktop host / emulator | `zemu_main.cpp` and `desktop_main.cpp` replay packets through the empty shell; they are **stubs, not hosts** | software lane |
| ARM runtime | `runtime/mister` contains only `.gitkeep` | G7 |
| MiSTer platform | `fpga/Zhaozhou.sv` is an inactive placeholder; `fpga/sys/` is intentionally empty | G7 |
| SDRAM controller | `zhao_sdram_ctrl.sv` exists, is synthesizable, has banked simulation/formal evidence; maturity held back pending hardware measurement | G7 |

**`zhao_prod_top` is a resource-counting harness, not the console.** Its blocks
receive separate LFSR-generated inputs rather than being connected to one
another, and its own header says its timing number has no functional-console
meaning. The release resource number must come from the functional platform
hierarchy (G8-A), not from this harness.

---

## 3. Deliberately deferred — do not re-litigate

Settled by owner ruling; listed so no agent re-opens them as "optional
features":

| Decision | Ruling |
|---|---|
| General tangent-space normal maps | **REFUSED in v1** — `OWNER-RULINGS-20260903-FUNDAMENTALS.md` D-8 |
| `GEOM.WARP` | **DEFERRED** — owner ruling 2026-08-31 §6.3, cut-order 5; its contract is deliberately unwritten |
| Generic cartridge resources | **Three families chosen (A)** — D-2; family pages become manifests, not private loaders |
| Cache coherence | **Generation-tagged caches (b)** — D-3 |
| Depth quantisation | **Separate named block** — D-4 |
| Fog | **Carry a factor, apply AFTER material shading** — D-5 |
| Spell lighting | Bounded; fireballs light the world — D-6 |
| Liquids | Ordinary geometry through the main renderer — D-7 |

**Terrain normal detail is NOT in this table.** It is a capability to protect and
implement, not an unanswered optional-feature question. Do not charge the
terrain's missing *ordinary* lighting to the normal-detail feature.

---

## 4. Evidence rules for anything claiming a pass

1. **Name the configuration.** Source revision, dependency closure, parameters,
   tool version, constraints, seed. A throughput result from a wide
   configuration and a resource result from a narrow default do not describe one
   machine.
2. **A row older than its source is not evidence.** 34 of 85 census rows were
   measured before their own `.sv` last changed (D19o). `cache_pipe` proved the
   cost: its stale row was the third-worst ALM entry in the census and described
   code that no longer existed.
3. **A killed or failed fit reports INCOMPLETE and keeps its synthesis
   evidence.** It must never become an empty row that passes a gate — which is
   exactly what happened before `check_fit_rules.ps1` was taught that a row with
   no resource numbers is unmeasured, not passing.
4. **A test must activate the hardware it claims to validate.** A reference-only
   CRC or a non-drawing shell bench produces a vacuous pass; the repository has
   already documented both.
5. **Fabric reserve is part of the pass.** On the provisional 41,910-ALM device a
   10% reserve is **37,719 ALMs**. An estimate below that line is not a pass, and
   framework/adapter costs are not subtractable.

---

## 5. What this file is not

It is not a schedule, not a design document, and not a substitute for the
contracts. When a capability's status changes, change the row — a release
definition that lags the manifest is worse than none, because it reads like a
decision.
