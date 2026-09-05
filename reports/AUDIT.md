# Zhaozhou reference / hardware alignment audit

Audit date: 5 September 2026. Repository: Fabulu/zhaozhou.

## Verdict

The repository contains substantial real reference work for new hardware, but not one coherent, up-to-date full-console reference. New per-block models, the legacy full-frame software renderer, the creature preview hooks, and the empty packet shell must not be treated as interchangeable evidence.

The strongest direct contradiction is not missing documentation: MATERIAL.COMBINE implements eight recipes, while MATERIAL.RESOLVE rejects recipes 6 and 7. These same two headers define incompatible types under the same fully qualified name, `zref::material::Ledger`. The residency model also admits a write footprint that overlaps a pinned page.

## Revisions and freshness

Initial detailed source audit:

- Hardware branch `zixxtrixx-v8-closeout`: `e1812993677e4997ac8cbea66216b614275e6594`.
- Main: `df5193cc00724e3edb2a0be5737ec709ab66bc6b`.

Final branch recheck:

- Hardware: `e007d97f69d80e32fdf9fae5a01719fda9fe03b8`.
- Main: `460490cfc29426037d8962d2e5b71d65daf73480`.

Between those hardware revisions, the entire `reference/include/zref` tree remained `a03dda1043e07ba88487680d69dedb635a17029e`; the `reference/include/zfield` tree remained `080a037843e96382ef73e5c2b58679b82469f6fb`.

The following full-file blob identities were rechecked at the later hardware revision:

| File | Blob SHA |
|---|---|
| `reference/src/zrender/rast.cpp` | `5a2e978668712031eaaade042218d5df6c8384a9` |
| `reference/src/zrender/render_frame.cpp` | `ad94fcb5e52a61dff2a01aa11e4b14d2d7c10d48` |
| `reference/src/zrender/terrain.cpp` | `ff92337197277becc9b4b94decd29d82780eb53f` |
| `tests/texture/island_composed_directed.cpp` | `f0e519d7ed3ad1119b33ea8d48fc58fea1549401` |
| `fpga/rtl/texture/zhao_texture_island_top.sv` | `b3fc15921cb2cf099bb31ec55021f482387ec8ee` |

Main's reference-header tree also remained unchanged at `c03f14a7a5ce2537a5cd4210b4f98399b3d494b5`. Thus the branch split and the key findings survived the final refresh. This is not a claim about later commits.

## Method and limits

Read the live repository through the GitHub connector: branch inventories, reference headers and implementation, selected RTL composition, actual test assertions, build inputs, and release/selection documents. Followed call sites instead of treating an existing header or a green status label as proof of integration.

No Quartus run, full repository build, or full Verilator/CI regression was executed. The residency counterexample was run locally against an explicitly transcribed executable subset of the two fetched headers. That extraction and its output are included for transparency. The supplied probe also targets the original checkout headers so the repository owner can validate it without using the transcription. The recipe and combined-header probes are provided but were not executed against an original checkout in this audit.

This is a source/evidence audit of the changing reference boundaries, not exhaustive verification of every RTL block.

## Findings

### R1 — High: the resolver rejects the combiner's new recipes

`zref_material.hpp` defines `kTerrainDetailLight = 6`, `kTerrainDetailMask = 7`, and `kRecipeCount = 8`. The scalar combine implementation executes both three-sample recipes. `material_combine_v1_diff.cpp` drives 200 cases for each of the eight recipes and compares returned RGBA/refusal against that oracle.

However, `zref_material_resolve.hpp::record_legal` still contains:

```cpp
if (recipe >= 6) return false;
```

A zero-initialized otherwise legal record with three samples and control `3 | (6 << 2)` or `3 | (7 << 2)` is therefore refused before reaching the combiner. The resolver tests exercise other recipe encodings but do not bridge this recipe-set mismatch.

This is an actual inter-model semantic contradiction. Do not describe eight-recipe material support as end-to-end until the record/resolve/combination route accepts and checks all eight.

Sources: [combiner](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/reference/include/zref/zref_material.hpp), [resolver](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/reference/include/zref/zref_material_resolve.hpp), [leaf differential test](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/tests/texture/material_combine_v1_diff.cpp), [resolver tests](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/tests/texture/material_resolve_directed.cpp).

Probe: `material_recipes_repro.cpp`.

### R2 — High: the two material headers cannot be combined as written

Both headers declare `zref::material::Ledger`, but the combiner's type contains refusal/saturation counters and the resolver's type contains cache/residency/refusal counters. The definitions are different and their header guards are different.

Including both in one translation unit redefines the same C++ type. This is a source-level compile conflict, not evidence that the current separate test targets fail. Give the two counters distinct names or one deliberately shared compatible type, then add a combined-header and resolve-to-combine test.

Sources: the same two headers as R1. Probe: `material_headers_smoke.cpp`.

### R3 — High: accepted depth profiles do not reach rendered depth

`zref_depth.hpp::depth_of_raw` implements the generated-profile, clamp, RCP24, scale, round-half-up, saturated 24-bit law.

The frame parser reads `SetView.flags[1:0]`, rejects reserved profile 3, and stores the accepted profile in `ViewSt::depth_profile`. The subsequent draw loop does not use the stored value.

`rast.cpp::project_vertex` still calculates exact-division Q16.16 `1/w`. Its header explicitly distinguishes that from the later invw24 law. Consequently, parsing and validating a profile is not proof that rendered occlusion observes it.

Sources: [depth oracle](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/reference/include/zref/zref_depth.hpp), [frame loop](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/reference/src/zrender/render_frame.cpp), [raster](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/reference/src/zrender/rast.cpp).

### R4 — High: the full-frame texture route is not the new material island

The full rasterizer interpolates UV affinely. The direct-colour creature path does call the common `Tmu::sample`, so it is wrong to say that the renderer has no TMU/filter/mip support at all. But using that sampler does not add perspective-correct interpolation or the complete material-record/resolve/multiple-sample/combine route.

The terrain path directly selects CI8 texels and palette colours, with mosaic selection and legacy modulation. It does not obtain full new material semantics by linking the new helpers into the library.

Sources: [raster](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/reference/src/zrender/rast.cpp), [TMU](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/reference/src/zrender/texture.cpp), [terrain](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/reference/src/zrender/terrain.cpp), [build inputs](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/reference/CMakeLists.txt).

### R5 — High: the composed texture test is not an exact-colour oracle comparison

The current test is meaningfully stronger than its earlier version: it exercises CLUT and bilinear classes, loads the palette properly, poisons invalid ingress cycles, checks recipe-job totals, and detects missing/duplicated/foreign tags.

Nevertheless, it checks nonzero colours rather than exact expected per-fragment RGBA. The top still extracts one low byte from each RGB565 texel for the bilinear lane and replicates the resulting byte to all three output channels. Full three-channel sequencing is explicitly unfinished. Palette expansion in the top is also a zero-fill shift rather than the reference expansion.

The top supplies identical UV/LOD values for all three material samples; the planner uses one global binding base/mode, so incrementing a binding index is not yet a complete independent per-sample binding lookup.

A counter moving or every colour being nonzero cannot establish that the fetched texels, weights, channel expansion, or material output are correct.

Sources: [top](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/fpga/rtl/texture/zhao_texture_island_top.sv), [composed test](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/tests/texture/island_composed_directed.cpp).

Additional source-level risk to test: bilinear fractions are connected directly from `plan_acc_fu/fv` while sample data returns through cache/dispatch latency. A varying-coordinate delayed-response test should establish that fractions stay attached to their sample. This audit did not execute a waveform test of that risk.

### R6 — High: known end-to-end order defect is permitted by a regression guard

The composed test documents an out-of-order tail and checks maximum displacement `<= 8`, not strict final submission order. This is openly labeled as a known defect; do not misrepresent it as concealed or as a strict ordering pass.

Internal completion reordering can be intentional—the combiner leaf test deliberately exercises it. The machine still needs the correct final ordering boundary wherever raster/blend semantics require it. A bounded existing defect is not proof of that boundary.

Source: [composed test, identity section](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/tests/texture/island_composed_directed.cpp#L450).

### R7 — High: particle format migration stops before expansion

`zref_particle.hpp` includes the new particle128 codec, six-bit relative-radius size, and `particle_radius(base_radius_fx16, size)`.

The same header's `expand_polygon` still implements the old screen-space size law (`side_sub = size << 4`). Its warning explicitly calls this superseded and not yet repaired. Two models agreeing with the old expansion do not establish conformance to the new world-radius contract.

Source: [particle reference](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/reference/include/zref/zref_particle.hpp).

### R8 — High: residency allocation admits overlap with a pinned neighbour

`Arena` allocates one free fixed-size page, but calls `upload_verdict` with the bounds of the entire arena. It never checks `length <= page_bytes_` and does not reserve multiple pages for a longer upload.

Counterexample, 256-byte pages at `0x1000`:

1. Publish A in page 0.
2. Publish B in page 1 and pin B.
3. Replace A in page 2, reclaiming unpinned page 0.
4. Publish C with length 512: page 0 is chosen and the full-arena bounds check accepts it.

C's accepted footprint is `[0x1000, 0x1200)`, overlapping pinned B at `[0x1100, 0x1200)`. The pin refusal counter stays zero. The model does not copy actual DMA bytes; the demonstrated fault is that its acceptance/allocation semantics authorize overlapping storage.

Local execution of the transcribed executable subset returned exit 1 and printed this overlap. A caller might separately restrict upload sizes, but the reference model itself does not enforce the property it is meant to represent.

Sources: [residency](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/reference/include/zref/zref_residency.hpp), [upload acceptance](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/reference/include/zref/zref_mem_upload.hpp). Probe: `residency_overlap_repro.cpp`.

### R9 — Partial: terrain normal-detail work exists but is not the finished feature

The unclamped directional shade primitive actually exists in `terrain.cpp`; the header narrative saying that the refactor is still blocked is stale.

The normal-detail scalar helper also exists. However, the full-frame preview has `kTerrainDetailStrength = 0`; when enabled it uses a per-triangle position-derived perturbation, not the final per-pixel world-anchored normal texture. It uses the fixed light constants in that path. Do not count this as the finished normal-detail feature or a completed moving-sun acceptance demonstration.

Sources: [shade implementation and preview](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/reference/src/zrender/terrain.cpp), [normal-detail helper](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/reference/include/zref/zref_terrain_normalmap.hpp).

### R10 — Partial: useful terrain and resource models are not full streaming

Real new reference work includes nested height decimation (33 to 17 and 9 samples), residency-set hashing, fresh-page publication and pins, and an F-sheet journal barrier that preserves dirty data until acknowledgement and reloads it on return.

The existing full renderer instead receives resident patch/resource pointers, composes a fixed lattice, and draws it. Its terrain route uses a painter sort with depth tests disabled while still writing depth. This is not the same as executing the full production residency/tessellation/upload/eviction pipeline.

Sources: [terrain helpers](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/reference/include/zref/zref_terrain.hpp), [journal model](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/reference/include/zref/zref_fjournal.hpp), [terrain draw](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/reference/src/zrender/terrain.cpp), [resource lookup/frame loop](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/reference/src/zrender/render_frame.cpp).

### R11 — Partial: full frame, preview hooks, and shell are different systems

The software frame loop still handles the Phase-3-style command subset. DrawProcedural implements the heightfield forge kind; DrawForm draws markers. The preview callback runs before resolve and can compose creatures/celestial work outside the normal resource-command route.

Separately, `ZhaoZrefShell::submit` calls `zhao_frame_execute_empty` and accumulates protocol counters. This does not mean that no software renderer exists; it means the empty-shell proof must not be cited as full graphical execution.

Sources: [frame loop](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/reference/src/zrender/render_frame.cpp), [empty shell](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/reference/src/zref.cpp).

### R12 — Partial: fog and post-gather support are not the full final compositor

The current decision is that fog applies after material shading. The audited full-frame raster path has no complete integrated new material-then-fog route. `zref_post.hpp` provides real gather arithmetic—glow accumulation/packing and bounded displacement—but those functions do not by themselves implement POST.COMPOSITE or prove that every preview effect follows its production path.

Sources: [post helper](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/reference/include/zref/zref_post.hpp), [raster](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/reference/src/zrender/rast.cpp), [release scope/rulings](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/design/V1-RELEASE-DEFINITION.md).

### R13 — Positive: Field v3 has a substantive current reference

`zfield_plan.cpp` implements the FPLAN lowering, uniform preparation, and vector reference executor around the same canonical semantic step layer. This is the appropriate separation: the functional oracle shares exact arithmetic, while RTL tests must separately establish scheduling, backpressure, ordering, and capacity.

The reel's ordinary terrain route still calls the canonical Field interpreter; it is not a benchmark of the v3 production scheduler. The manifest distinguishes experimental composition from the still-unfinished production executor.

Sources: [v3 planner](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/reference/src/zfield/zfield_plan.cpp), [terrain Field call path](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/reference/src/zrender/terrain.cpp), [selection manifest](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/design/prod_manifest.yml).

### R14 — Positive but not composed: geometry and asset boundaries have actual models

The geometry reference includes canonical 32-byte vertex decoding and parameter-buffer validation. `zref_assetfetch.hpp` provides real footprint/address/admission/refusal rules for the meshlet asset stream. Their existence corrects stale release prose saying some of this work is entirely unbuilt; it does not turn the older DrawForm marker path into the complete mesh-fetch/decode/project/render path.

Sources: [geometry reference](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/reference/include/zref/zref_geom.hpp), [asset fetch](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/reference/include/zref/zref_assetfetch.hpp), [frame loop](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/reference/src/zrender/render_frame.cpp).

### R15 — Medium: cost metadata has drifted independently of pixels

`product_jobs(kMask)` returns 1, labeled an alpha product. The combine implementation's MASK recipe is a binary alpha gate; the composed test's local expected table assigns MASK zero jobs. Its comment references the oracle table, but the test actually hardcodes its own table. Fix and test this inconsistency; it is not evidence of a pixel mismatch by itself.

Sources: [cost helper](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/reference/include/zref/zref_material.hpp), [MASK implementation](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/reference/include/zref/zref_material.hpp#L320), [test job table](https://github.com/Fabulu/zhaozhou/blob/e1812993677e4997ac8cbea66216b614275e6594/tests/texture/island_composed_directed.cpp#L390).

## Branch split

The main branch lacks newer hardware-branch headers such as material combination, material resolution, resource residency/upload, the F journal, asset/mesh fetch, post gather, and terrain normal detail. It has newer creature authoring work of its own. Treat these as two real trees, not one imaginary combined release.

Header inventories: [hardware](https://github.com/Fabulu/zhaozhou/tree/e007d97f69d80e32fdf9fae5a01719fda9fe03b8/reference/include/zref), [main](https://github.com/Fabulu/zhaozhou/tree/460490cfc29426037d8962d2e5b71d65daf73480/reference/include/zref).

## Recommended acceptance order

First repair the hard inter-model contradictions (R1, R2, R8, and the R15 metadata discrepancy). Add tests crossing these boundaries rather than just enlarging separate suites.

Then establish one current functional frame route, consuming the canonical depth law, perspective-correct attributes, material resolution/recipes, particle radius law, and required post-material stages. Keep the old preview/capture route explicitly versioned if it remains useful; do not silently call it the new machine or blindly overwrite old golden CRCs.

For hardware compositions, compare exact per-transaction results by identity under varying attributes, cache delays, output stalls, and final drain. Deliberately distinct sample textures and channels are essential. Include the final ordering boundary where order affects the image.

Finally connect asset/resource/terrain lifecycle models to the frame route and prove representative complete scenes, including eviction and return. Maintain separate functional, capacity, and physical verdicts.

Do not make the functional reference simulate every flip-flop merely because hardware was retimed. Preserve an independent, practical semantic oracle; model transport and timing where they are observable and test physical throughput in the actual RTL configuration.
