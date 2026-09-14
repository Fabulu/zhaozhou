# Zhaozhou: a conditional path through the resource ceiling
Date: 14 September 2026  
Pinned ceiling branch: `claude/ceiling-architecture-20260912`  
Read/rechecked head: `3e1e8b752806eb9b17dbcc5704c127f1672acbba`

## Verdict

A coherent design route exists to investigate. There is NOT yet a demonstrated complete resource-and-throughput portfolio. The route is to eliminate duplicated engines and oversized per-clock service obligations, not to erase visual capabilities, hide unpriced functionality, or relabel guesses as fitted area.

The previous approximately 15k savings and 15.3k completion estimates were engineering scenarios, not physical lower bounds. In particular, standalone unbuilt-block ceilings do not prove a shared/folded implementation must cost the same. Conversely, memory conversion or resource sharing is not free.

No repository files were changed and no RTL build, Quartus fit or board load was run in this review. Independent Python integer-algebra checks are included and narrowly labelled.

## 1. Capacity and the relevant target

Use the repository's selected Quartus device budget: 41,910 ALMs, 112 DSP blocks, 553 M10Ks. The charter already requires at least 10% fabric reserve until the complete core is stable [S1]. Applying that to ALMs gives:

- physical capacity: 41,910;
- 90% working limit: 37,719;
- extra allocation over the tightened 30,000 target while retaining that reserve: 7,719;
- illustrative complete design-to-cost portfolio below: 37,500;
- physical reserve at 37,500: 4,410 (10.52%);
- flexibility before crossing 37,719: only 219.

The 4,410 is reserve, NOT an unpriced-function pot. A near-41k design has almost no such reserve and a low ALM percentage alone never proves timing. The protected legacy shell already misses its timing constraint [S16].

This proposal does not silently change the 85-DSP objective. The 112 physical DSP capacity is not an automatic authorization to spend it. A suggested M10K planning limit of 480 is a proposal, not a mapped portfolio; memories still require a full port/width/depth/replication ledger.

## 2. Non-overlapping whole-machine design-to-cost envelopes

**These are required implementation objectives, not a fresh forecast or fitted bill.** They deliberately include all previously unwritten and unpriced required functions. Shared providers are charged exactly once. The module-by-module ownership assignment still has to be checked by the implementation agent; an omitted function is an error, not a free saving.

| Physical group | ALM objective | Inclusive scope |
|---|---:|---|
| Backend/platform | 14,000 | Command/scheduler, guard and memory transports, upload/material lookup, binner/attribute production, raster/Early-Z, complete selected texture/material/fog/ink path, tilestore/resolve/framebuffer/video/audio/input, mandatory diagnostics, and one thin board wrapper. Excludes separately listed geometry front end, projection, post and 2D. |
| Shared projection and replay | 5,000 | Exactly one projection service, context and result arenas, replay ownership; no duplicate private geom/terrain projectors. |
| Geometry and lighting | 5,500 | Geometry asset front end, decode/assembly, pose/cache, skin, normal/magnitude generation, cull/LOD/clip/setup/depth quantization and complete RGB/multi-light/emission producer. Attribute raster transport charged in Backend/platform. |
| Terrain/Forge/surfaces | 4,500 | World/page/load/residency/compose/mip/bake/LOD/normals/TESS maintenance, terrain sequencing, Forge primitive evaluation/cliffs/shadows, and surface operations. Projection, common lighting, shared upload and raster excluded because charged elsewhere. |
| Complete FIELD | 4,500 | All selected physical providers, register file, directory, tables, executor, profile walkers, queues and result transport; no unpriced providers outside this envelope. |
| Complete particles | 2,000 | State streaming/compaction, species store, update/collision/spawn, representation/expansion/record/soft endpoint and bounded flow interface. FIELD arithmetic provider and raster separately charged. |
| Post and 2D | 2,000 | Gather, bounded post stream, displacement, bloom/haze/grade/flash/ink/HUD, 2D planes/sprites, required memories and interfaces. Any selected optional echo included here rather than silently excluded. |
| TOTAL | 37,500 | Complete selected scope, including one platform wrapper |

This table is a falsifiable constraint, not proof of feasibility. The backend, geometry/lighting, complete FIELD and terrain objectives are particularly demanding. If the backend costs 17k rather than 14k, the extra 3k must be recovered from a named owner or this plan misses the 10% reserve. Do not call a 40.5k result closure merely because it is below 41,910.

## 3. The main mechanism: visible service requirements, not universal II=1

Preserve the required population, exact trajectories, 60Hz presentation, armies, deformation, split-screen and visual effects. Propose explicit amendments only to internal latency/acceptance contracts where a complete calendar proves that slower acceptance still delivers identical results before the required consumer deadlines.

At 100MHz and 60Hz, a conservative raw compute frame is 1,666,666 clocks [S3]. For the 32,768-particle tier [S4-S7], issue-only cost is:

| Combined service initiation interval | Issue clocks | Milliseconds at 100MHz |
|---|---:|---:|
| 1 | 32,768 | 0.32768 |
| 4 | 131,072 | 1.31072 |
| 8 | 262,144 | 2.62144 |
| 12 | 393,216 | 3.93216 |
| 16 | 524,288 | 5.24288 |

These are NOT complete particle-system timings. They exclude fill/drain, memory arbitration, terrain-sample service, flow evaluation and spawning. An II8 combined service is a candidate to build a schedule for, not a declaration that eight clocks execute every recipe.

Use a small fixed recipe sequencer and bounded datapath, not one spatial pipeline per recipe and not a second general FIELD CPU. Keep species, waiting jobs and scratch in synchronous RAM; retain active operands and truly concurrent control in fabric. Keep one state/compaction owner and preserve survivor/child ordering. Bound refused spawning without computing arbitrary rejected children; prove event/count semantics. Do not multiply simulation work by two for Duo: shared simulation is already a charter requirement, not a new saving [S1].

A proposed 2k-3k complete particle implementation is a worthwhile design target versus the earlier 4.5k completion allowance. The 2k whole-domain quota above is the demanding end, includes existing endpoints, and must not be called measured.

## 4. A concrete exact compositor optimization

POST.COMPOSITE specifies three generated per-channel curves, with index sizes 32/64/32, followed by a signed 3x3 Q2.14 matrix. It allows generated fusion only when exact; it specifically rejects a giant 65,536-entry RGB565 remap [S8].

Let c_R(r), c_G(g), c_B(b) be the three independent curves. For each configuration, precompute **unrounded product vectors**:

```
T_R[r] = c_R(r) * M[:,0]
T_G[g] = c_G(g) * M[:,1]
T_B[b] = c_B(b) * M[:,2]

pre_round_RGB = T_R[r] + T_G[g] + T_B[b]
```

Then apply the original bias, full-width accumulation rules, single rounding and saturation unchanged. This is an exact distributive identity, not an approximation. It replaces nine per-pixel multiplication operations with three independent small table reads and addition. There is no intermediate table-entry rounding.

Under the explicitly checked unsigned-colour8 / signed-coefficient16 domain, each product fits signed24. Each table entry is three signed24 components, or 72 bits. Logical table capacity is `(32+64+32)*72 = 9,216 bits`.

**That does not mean one M10K.** Three simultaneous 72-bit read ports suggest two 256x40 ROM/SDP slices per table, six physical M10Ks initially. Count active/inactive configuration epochs, source line rings, glow/displacement storage and all port copies. The old whole-post eight-M10K ceiling cannot be assumed to remain valid.

The independent Python check evaluated every RGB565 input for 16 curve/matrix configurations, including signed coefficient rails: 1,048,576 input/configuration pairs, zero mismatching full pre-round RGB sums. Incrementing one red-table entry by one produced 2,048 mismatching colours. This verifies the stated integer identity in that domain, NOT the full repository post oracle, timing, ALM reduction or a DSP fit.

The raw matrix products disappear structurally in this proposed implementation. Net ALM benefit is still uncertain because table addressing, pipeline and adders cost logic. This is especially attractive as a DSP rescue that need not slow the colour transform.

A separate alternative is a bounded three-product worker reused over three issue phases per pixel. For Duo, `98,304*3 + 15,360 = 310,272` clocks is only an issue/glow-work lower bound. It is NOT a complete post timing claim. Since post follows resolve under an exclusive lease, it leaves only 1,023,061 of a 1,333,333-clock window for preceding serial work before other overheads. Do not spend a complete independent frame on both rendering and post. The LUT and time-shared alternatives overlap and must not be credited twice.

## 5. Protect the saturated machinery

The golden route is not to serialize everything.

Dense two-view replay requires 663,552 terrain fills in the inspected profile [S12]. At II3 that is 1,990,656 issue clocks BEFORE geometry. Including the 120,000 geometry-vertex workload, even a combined II2 service needs 1,567,104 issue clocks before stalls, beyond a 20%-reserved frame window. Keep the legal throughput-capable projector and remove the redundant projector through reuse rather than resuscitating the illegal RPP1 point.

Likewise, do not cut FIELD's existing four physical multiplier lanes without a complete accepted-program schedule. Its directory, queues, register-file ports, exact roots, executor and profile adapters all count within the one complete-domain allocation [S2,S17].

For lighting, calculate world-space work once at its proper identity. Reuse a normal/magnitude across independent lights only where the current law permits, then preserve per-light clamp, gain/emission and final saturation order [S9-S10]. Do not cache camera-dependent results under a view-independent key. Shared world work is not evidence that an unchanged scalar 147-clock service meets 480k light evaluations/frame; it does not.

## 6. Storage changes that are still legal

Keep cold records in RAM, move compact handles through pipelines, reuse immutable source records until the last reader releases them, and count every physical copy. These are source-lifetime changes, not permission to shorten queues arbitrarily.

The texture owner already stores wide payloads in statically banked RAM and deliberately retains its small, genuinely concurrent scoreboards in fabric [S13]. Do not claim to move those already-inferred payloads again, or force multi-update state through an impossible single-port RAM.

Packet D's 1,157-bit metadata record is a useful pressure point, not a license to prune required attributes. At its 29-by-40-bit slice arrangement, a <=256-deep profile implies 29 initial M10K slices; 512-deep implies 58 before other banks. The old binner default is 128 triangles [S14-S15]. A full game-scale design must qualify its selected capacities; growing the default is not free.

The three-plane D/G8A characterization does not cover complete Gouraud, fog, emission and AUX world-coordinate profiles. Charge their production, transport and read ports at the final selected boundary. Do not interpret a small three-plane fit as the final cel-shaded game.

A thin board wrapper is required within the backend allocation. It must preserve the essential clock/reset/HPS/memory/video/control functions once, not retain two video systems and subtract one on paper. The full MiSTer test image is useful for bring-up, not evidence that the minimal platform is already built.

## 7. Provider calendars and early failure conditions

For every physical provider p:

```
C_p = sum_over_clients(required_jobs * issue_slots_per_job)
      + setup + fill/drain + arbitration + memory stalls
```

With multiple lanes, use a proved schedule rather than dividing blindly; dependent operations, ports and tagged completion can prevent perfect utilization.

Every provider must finish within its own allowed phase windows. Then check the end-to-end frame dependency path. Independent engines can overlap; post's exclusive final lease cannot overlap whatever the contract forbids. Never count the same slack twice.

Reject or redesign the portfolio when:
- a required function has no physical-owner row;
- one owner or RAM instance is billed twice;
- a workload is a synthetic stress mislabelled as a requirement, or vice versa;
- an unfitted target is quoted as a measurement;
- an old test oracle substitutes for current rounding law;
- ALM decreases while DSP, RAM ports, timing or bandwidth violate their limits;
- the 10% reserve is spent on still-unpriced functionality;
- assumed frame work depends on absent occupancy/camera/light/cache traces.

## 8. Execution order without restarting the project

Keep Packet D/E progressing toward the already named G8A boundary. This proposal does not authorize a new Quartus campaign or a diversion into unrelated micro-fits.

Alongside that work, one bounded architecture packet should:
1. produce a complete owner/function allocation including unbuilt requirements and board;
2. derive mandatory per-frame/per-phase workloads and propose explicit internal throughput amendments;
3. qualify exact compositor fusion and concrete particle schedules in simulation/reference tests;
4. freeze the selected memory geometries and provider calendars;
5. attach each unpriced objective to the existing appropriate subsystem fit/adoption gate.

Reuse the paid legacy shell evidence. Measure the next actual selected connected boundaries, compare like-for-like subtrees, and update this design-to-cost table after each gate. If a large owner misses its quota, redistribute only against a named demonstrated alternative—not optimism.

## Conclusion

The credible route is **same visible machine, fewer permanent arithmetic engines, smaller active state, more deliberately laid-out RAM, and lower internal service rates where actual deadlines allow**.

There is enough structure in the workload and arithmetic to make this a serious plan rather than a wish. There is not enough composed physical evidence to certify the 37.5k table, the unchanged 85-DSP target, the full memory portfolio and frame timing simultaneously. The backend, full lighting schedule and complete FIELD remain the decisive risks.

## Sources
All repository sources below are pinned to `3e1e8b752806eb9b17dbcc5704c127f1672acbba`.
- [S1] `ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md` — Mandatory visible capabilities, shared Duo simulation and >=10% fabric reserve.
  https://github.com/Fabulu/zhaozhou/blob/3e1e8b752806eb9b17dbcc5704c127f1672acbba/ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md
- [S2] `reports/RESOURCE-RESCUE-ROADMAP-CURRENT-20260913.md` — R0-R9 state; measured/built/adopted distinctions and scope.
  https://github.com/Fabulu/zhaozhou/blob/3e1e8b752806eb9b17dbcc5704c127f1672acbba/reports/RESOURCE-RESCUE-ROADMAP-CURRENT-20260913.md
- [S3] `design/budgets/workloads.yml` — 100MHz/60Hz frame definition; named demand qualifications.
  https://github.com/Fabulu/zhaozhou/blob/3e1e8b752806eb9b17dbcc5704c127f1672acbba/design/budgets/workloads.yml
- [S4] `design/contracts/PART.STATE.md` — 32768 population; HPS DDR streams, compaction and deterministic priorities.
  https://github.com/Fabulu/zhaozhou/blob/3e1e8b752806eb9b17dbcc5704c127f1672acbba/design/contracts/PART.STATE.md
- [S5] `design/contracts/PART.UPDATE.md` — II=1 contract and sequenced-vs-spatial implementation trade; recipe ordering.
  https://github.com/Fabulu/zhaozhou/blob/3e1e8b752806eb9b17dbcc5704c127f1672acbba/design/contracts/PART.UPDATE.md
- [S6] `design/contracts/PART.COLLIDE.md` — Collision response law and existing II=1 contract.
  https://github.com/Fabulu/zhaozhou/blob/3e1e8b752806eb9b17dbcc5704c127f1672acbba/design/contracts/PART.COLLIDE.md
- [S7] `design/contracts/PART.SPAWN.md` — Bounded spawn groups, deterministic ordering and one generation/tick.
  https://github.com/Fabulu/zhaozhou/blob/3e1e8b752806eb9b17dbcc5704c127f1672acbba/design/contracts/PART.SPAWN.md
- [S8] `design/contracts/POST.COMPOSITE.md` — Generated curves+matrix, exact-fusion permission, one-pass/line-ring rules.
  https://github.com/Fabulu/zhaozhou/blob/3e1e8b752806eb9b17dbcc5704c127f1672acbba/design/contracts/POST.COMPOSITE.md
- [S9] `design/contracts/GEOM.LIGHT.md` — Shared core versus unfinished multi-light colour shell and emission.
  https://github.com/Fabulu/zhaozhou/blob/3e1e8b752806eb9b17dbcc5704c127f1672acbba/design/contracts/GEOM.LIGHT.md
- [S10] `reports/SHADE-LIGHT-CONSOLIDATION-20260909.md` — Core ownership, arithmetic differences and scalar initiation interval.
  https://github.com/Fabulu/zhaozhou/blob/3e1e8b752806eb9b17dbcc5704c127f1672acbba/reports/SHADE-LIGHT-CONSOLIDATION-20260909.md
- [S11] `fpga/rtl/common/zhao_project_core.sv` — Duplicated core history, three exact quotients and throughput.
  https://github.com/Fabulu/zhaozhou/blob/3e1e8b752806eb9b17dbcc5704c127f1672acbba/fpga/rtl/common/zhao_project_core.sv
- [S12] `reports/CEILING-FRONTIER-RECONCILIATION-20260912.md` — Rejects RPP=1 dense two-view point; 111 conditional DSP frontier.
  https://github.com/Fabulu/zhaozhou/blob/3e1e8b752806eb9b17dbcc5704c127f1672acbba/reports/CEILING-FRONTIER-RECONCILIATION-20260912.md
- [S13] `fpga/rtl/texture/zhao_texture_v3own.sv` — RAM payloads, concurrent fabric scoreboards, read-late ownership.
  https://github.com/Fabulu/zhaozhou/blob/3e1e8b752806eb9b17dbcc5704c127f1672acbba/fpga/rtl/texture/zhao_texture_v3own.sv
- [S14] `reports/PACKET-D-ATTRIBUTE-RASTER-ABI-20260914.md` — Current rounded-gradient law, three-plane scope, 1157-bit record.
  https://github.com/Fabulu/zhaozhou/blob/3e1e8b752806eb9b17dbcc5704c127f1672acbba/reports/PACKET-D-ATTRIBUTE-RASTER-ABI-20260914.md
- [S15] `fpga/rtl/geometry/zhao_geom_binner.sv` — 128-entry default is not a full content-scale memory/workload receipt.
  https://github.com/Fabulu/zhaozhou/blob/3e1e8b752806eb9b17dbcc5704c127f1672acbba/fpga/rtl/geometry/zhao_geom_binner.sv
- [S16] `reports/synthesis/zhao_shell_fit.json` — Clean protected legacy-shell attribution and timing; not whole-console evidence.
  https://github.com/Fabulu/zhaozhou/blob/3e1e8b752806eb9b17dbcc5704c127f1672acbba/reports/synthesis/zhao_shell_fit.json
- [S17] `ZHAOZHOU_MEMORY_FIRST_RESOURCE_RESCUE_2026-09-09.txt` — Arithmetic/storage alternatives; provider calendars and global resource acceptance.
  https://github.com/Fabulu/zhaozhou/blob/3e1e8b752806eb9b17dbcc5704c127f1672acbba/ZHAOZHOU_MEMORY_FIRST_RESOURCE_RESCUE_2026-09-09.txt
- [S18] `design/contracts/MATERIAL.RESOLVE.md` — Lookup/cache only; shared material execution elsewhere.
  https://github.com/Fabulu/zhaozhou/blob/3e1e8b752806eb9b17dbcc5704c127f1672acbba/design/contracts/MATERIAL.RESOLVE.md
- [S19] `design/contracts/MEM.UPLOAD.md` — General upload owner and surrounding guard/arbitration/pin obligations.
  https://github.com/Fabulu/zhaozhou/blob/3e1e8b752806eb9b17dbcc5704c127f1672acbba/design/contracts/MEM.UPLOAD.md
- [S20] `design/contracts/FORGE.SHADOW.md` — Ordinary geometry, no separate shadow-map/raster engine.
  https://github.com/Fabulu/zhaozhou/blob/3e1e8b752806eb9b17dbcc5704c127f1672acbba/design/contracts/FORGE.SHADOW.md

Vendor memory geometry: Cyclone V Device Handbook, M10K mixed-width configurations:
https://docs.altera.com/r/docs/683375/current/cyclone-v-device-handbook-volume-1-device-interfaces-and-integration/m10k-blocks-mixed-width-configurations
