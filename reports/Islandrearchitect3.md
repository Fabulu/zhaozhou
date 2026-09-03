Skip to content
Fabulu
zhaozhou
Repository navigation
Code
Issues
Pull requests
Agents
zhaozhou/reports
/Islandrearchitect.md
Fabulu
Fabulu
24 minutes ago
1173 lines (650 loc) · 54.9 KB

Preview

Code

Blame
this is bros summary of the plan: Done. The full architecture brief is 1,704 lines / 57.6 KB and includes the corrected buildability ledger, binding rulings, recent-RTL review, complete 8 km terrain-world decisions, exact integration gates, and the revised build order.

Download the full Zhaozhou buildability and architecture brief

Main verdict The submitted audit was useful, but not safe to ratify unchanged. Several of its supposed owner questions had already been answered by the binding August 31 rulings:

Depth is already fixed as three named profiles, with generated and proved scales. You do not need to choose wmin, wmax, or scale again.

276,480 already means pre-Early-Z covered fragments for Z60 at 3× overdraw. It is not a TMU sample count.

Particle overflow, spawn ordering, collision sources and responses, recipe vocabulary, update order, and representation ladder are already substantially frozen.

TWOD.SPRITE is already HUD-only and shares the primary TMU; TWOD.PLANE is already the restricted world-plane engine; post-processing order is already frozen.

MEASURE.HISTOGRAM should remain deliberately refused.

Conversely, the claim that zero contracts remain accidentally unwritten is false. SW.STREAM, the software side that must prepare and stream the island world, still contains substantive TODO sections. And the August 31 architecture requires an external-SDRAM GEOM.PARAMBUF, but no corresponding ledger entry, contract, reference implementation, or RTL currently exists.

Rulings made Depth and fragment budgets Depth remains:

WORLD_LONG: 1 m–16,384 m, scale 2^40

WORLD_STANDARD: 0.5 m–8,192 m, scale 2^39

CLOSE: 0.25 m–2,048 m, scale 2^38

SetView.flags[1:0] already carries the profile. wmax is a depth clamp, not a far clipping plane. The old open-depth report should be marked superseded.

The canonical workload distinction is now:

320,000 pre-Early-Z fragments/frame as the cross-mode design target.

Post-Z survivors are a separate measured trace.

Material samples are a separate measured trace.

AUX requests are a separate measured trace.

1,333,333 clocks/frame is the performance-design budget.

1,666,667 clocks/frame is the late-frame fault boundary, not the target.

Particle path The particle behaviours were already specified. The true missing law was numeric interpretation. I ruled:

Position: signed S9.8 metres relative to a population origin — 1/256 m resolution and roughly ±512 m local range.

Velocity: signed S2.8 metres per 60 Hz tick — roughly ±240 m/s.

Size: unsigned U2.4, interpreted as a relative radius scale.

Spin: U0.6 turns.

Population origins are fx16 and may be deterministically rebased between ticks.

Existing survivors outrank children; never evict the oldest survivor to admit a spawn.

Terrain and simple planes are the FPGA collision sources; creature/gameplay collision remains HPS-owned.

This is capture-visible numeric law, so the brief calls for QFMT_VERSION 2 → 3 and a particle format version.

2D and post-processing The plane engine receives two explicit roles:

BACKDROP: no scene-depth interaction.

ATMOSPHERE: bounded alpha/additive overlay with optional world anchoring.

Water or lava that must genuinely intersect world geometry uses ordinary triangles. The restricted plane engine must not turn into a second general TMU.

POST.GATHER’s current memory arithmetic is internally inconsistent: the stated wide accumulators do not fit its approximately 31 KB / ≤10-M10K claim. I ruled a tile-local wide accumulator followed by compact global quarter-resolution storage:

RGB565 glow

signed 8-bit X displacement, bounded ±8 pixels

signed 8-bit Y displacement, bounded ±4 scanlines

one-bit ink mask

That makes the real global storage approximately 30 M10Ks in Duo, not ≤10.

POST.COMPOSITE should be a bounded line-streaming pipeline with a displacement line ring, not five complete framebuffer rereads. Generic exact 65,536-entry RGB565 grading tables are rejected for v1; use generated per-channel curves plus a small exact matrix/bias transform.

Binner and GEOM.PARAMBUF I reject the new suggestion to solve binner capacity merely by choosing 8/16/32/64 full-mesh creatures and growing the on-chip arena. The newer LOD measurements are genuinely good news, but they do not supersede the binding external-parameter-buffer decision.

The ruling is:

Build GEOM.PARAMBUF in local SDRAM under ENGINE1.

Keep only directories, active tails, prefetch FIFOs, and opportunistic caches on chip.

Guarantee 32 ordinary full-mesh creatures machine-wide, with at least 16 available to either active Duo view.

Treat the giant separately with a dedicated 32,768-reference reserve; it must not consume the ordinary creature quota.

Unexpected hard overflow repeats the previous complete frame rather than publishing an arbitrary missing tail.

Three-sample materials Three-sample terrain is a real shipping capability, but not a promise that every visible terrain fragment always receives all three samples.

The quality ladder is:

base + detail + light/mask

base + detail

base only

Gouraud/glint fallback where permitted

The governor chooses the level before sealing the frame. The 1,094,600-sample profile remains an upper-envelope stress gate, while real scheduling must use post-Early-Z material-request traces.

I also froze exact native combiner operations for passthrough, modulate, modulate-2×, lerp, saturated add, mask, and terrain detail/light combinations, all using the machine’s existing unit8 /256 arithmetic. The current “sample 0 plus combiner_unfrozen” implementation is useful scaffolding but is not a finished material path.

Problems in today’s new texture RTL The recent work is conceptually strong, but it is not yet ready to integrate merely because the directed leaf tests are green.

PERSPUV The reported 415,627 fragments/frame and 50% headroom assumes one recovered UV pair per fragment. That is not sufficient evidence for the ratified three-sample architecture. Three independent UV sets would require:

1,094,600 × 2 = 2,189,200 product launches/frame,

which cannot pass through one one-product-per-clock lane inside the 1,333,333-clock design budget.

The production ruling is therefore two parallel product paths—U and V concurrently—yielding one perspective pair per clock, unless traces prove that secondary terrain samples always share an already-recovered coordinate pair. The existing scheduled reciprocal remains sensible.

TEXJOIN v2 The current implementation has several concrete problems:

It allocates after perspective, even though transaction ownership should begin immediately after Early-Z.

It uses a two-bit generation, wrapping after four slot reuses; production uses eight bits.

free_cnt_q is independently decremented on acceptance and incremented on retirement in the same always_ff; same-cycle acceptance and retirement lose one update. This is the exact bookkeeping fault that the PERSPUV tests already found in another block.

Its tests do not currently exercise that steady-state same-cycle case.

It performs broad combinational scans across all entries to choose TMU and AUX work.

TMU and AUX return paths are declared permanently ready without skid capacity or duplicate-return protection.

A zero-sample fragment can expose uninitialised sample-0 storage.

Its output is combinational from the table head rather than registered.

The material combiner remains deliberately unfinished.

The brief specifies a production replacement while keeping this implementation as a useful architectural prototype.

TMU planner, palette and cache The elastic TMU planner’s corrected 0/357 address comparison is good, but it currently proves only the non-CLUT planning subset. Before adoption it needs the complete inherited texture-format suite, including CLUT behaviour, palette residency, mip edges, wrap modes, stalls, repeated external IDs, and out-of-order completion.

The resident palette needs a transactional BEGIN / WRITE / END load protocol, CRC, eight-bit generations, and same-slot lookup exclusion while loading.

The cache’s same-line fill multicast is an excellent correction, but the new source still performs combinational array reads and compares rather than the required:

synchronous RAM read → fabric capture → registered classify

That may prevent the intended M10K implementation and recreate the RAM-output timing problem this rearchitecture was supposed to remove. The FIFO pointer widths are also effectively hard-coded around the present depth.

Terrain residency The direct-mapped 1,024-slot residency prototype should not be integrated as production architecture.

It lacks island identity in its key, has a deterministic 2,048 m collision period, has only four-bit generations, does not reserve dirty victims until writeback finishes, has no pin/in-flight protection, cannot unload without global reset, and its metadata access pattern is not a proper synchronous-M10K directory. The current source has repaired one same-cycle loader-finish hazard, but the broader transaction problem remains.

The brief rules a TERRAIN.RESIDENCY_V2:

256 sets × four ways

full {resource_epoch, island_id, ix, iz} tags

deterministic round-robin victim selection

eight-bit generation

explicit EMPTY / LOADING / RESIDENT_CLEAN / RESIDENT_DIRTY / WRITEBACK states

in-flight pin counts

dirty writeback acknowledgment before reuse

synchronous metadata RAM

explicit epoch teardown

8 km terrain-world rulings The file provides a complete memory and command architecture rather than leaving ten more questions for a later brief. Major decisions include:

Full patch identity includes resource_epoch, island_id, patch_ix, and patch_iz.

Static authored pages use CPU-canonical scar/breach state.

FPGA writes back mutable surface-sheet layer F; B/D remain CPU canonical.

Dynamic composed-height cache remains 256 patches, while static terrain can bypass it.

Prefetch policy: current visible set + one-patch ring + bounded 30-frame velocity prediction.

Initial prefetch cap: 32 complete pages/frame, treated as provisional until board bandwidth evidence.

Coarse height mips are generated on the FPGA by exact 33→17→9 sample selection, not averaging.

Local SDRAM bank 2 receives a concrete terrain-world map, while ENGINE1 remains render-critical and a new best-effort TERRAIN_BUILD client handles page loading and writeback.

Two additive terrain command records are specified: epoch/control and deterministic terrain-set submission.

A dense 8×8 km plate at 2 m pitch still cannot fit in 128 MB; the product target remains a sparse streamed world with 1,024 resident patches.

Immediate build order Fix the block-fit source manifests. The most recent apparent Quartus failure was not a failed design: the generated QSF omitted the new module and its dependencies because run_block_fit -Module does not add sources by itself.

Repair and fit the texture leaves individually.

Build one real texture-survivor composition: Early-Z → token allocation → reciprocal/perspective → multi-sample issue → cache/TMU/palette → AUX → exact combiner → registered retirement.

Meet the staged physical gates: approximately 150 MHz leaves, 120–125 MHz islands, 115–120 MHz texture-survivor composition.

Build and integrate external GEOM.PARAMBUF.

Only then build terrain-world streaming and residency v2.

Particles, planes, gather and post follow after the conventional full renderer is physically credible.

The hierarchy remains:

105 MHz full-composition acceptance floor

110 MHz architecture objective

115 MHz stretch/headroom

100 MHz product clock

The central conclusion is:

Zhaozhou is not blocked by dozens of unanswered game-design questions. It is blocked by a much smaller set of concrete numeric, storage, transaction and physical-composition problems—and several of today’s “finished leaves” still need one more architecture pass before they are allowed into the machine.

The text file is deliberately marked proposed owner rulings. Committing or otherwise adopting it would make those decisions binding; I did not modify the repository.

ZHAOZHOU_BUILDABILITY_RULINGS_2026-09-02.txt Dokument

Big fucking news. The texture fit finally answered the question.

Hardware: the first texture architecture does not hold 100 MHz All 10 texture-island blocks have now been physically fitted. The answer in the agent's own report is literally:

“No.” “It needs more reengineering.”

The one-seed leaf results are:

Block Fmax AUX pipe 54.95 MHz TEXJOIN v2 61.66 PERSPUV svc 62.67 RCP24 svc 68.46 Cache pipe 81.06 Mosaic 86.63 TMU planner 93.55 Bilerp lane 99.69 Palette resident 104.42 Response dispatcher 110.90 So: 8/10 are slower than the existing 99.50 MHz renderer, and none reaches the 120–125 MHz island target, never mind 150 MHz leaf headroom.

But this is not “the console is 54.95 MHz.” These are isolated virtual-I/O leaf fits, not the composed machine. Several of the worst numbers are already known to be measuring architecture we have explicitly ordered him to replace. Composition generally makes things worse, but these exact numbers are not the future architecture's numbers.

And this is why making him FIT THE DAMN THING NOW was the right call.

The really useful part: TimeQuest named the actual defects AUX 54.95: its worst path literally starts at an input pin and runs through clamp/normalisation into the divider. The “pipelined” block had no registered input seam. He's already inserted one: A0 captures subtract/shift, A0b does clamp/mux. II stays 1. That 54.95 is now explicitly a before number.

TEXJOIN 61.66: our feared 16×3 combinational issue scan is absolutely real. The path is 13 logic levels / ~15.7 ns, mostly routing. He's now replaced the scan with a 64-entry work FIFO, preserving oldest-first issue order while removing the giant priority search. TMU/AUX/retirement outputs are registered too, and zero-sample retirement was fixed with a test that actually fails on the old implementation. 33 checks pass. Not refitted yet.

PERSPUV 62.67: product register → variable rescale → saturation → entry storage is one enormous ~15 ns cone. This is exactly the other half of what the brief told him to repair. And remember: this is before the production two-parallel-product-lane redesign, so PERSPUV clearly needs real surgery rather than merely another copy.

Cache 81.06: this one is spectacular confirmation of our review. It uses 5,634 ALMs and 10,812 registers with only 3 M10Ks. In other words: yes, the supposed cache arrays really did become a gigantic sea of flip-flops because of the combinational read structure. The C0→C4 synchronous RAM/capture/classify architecture isn't optional; it's the path to fix both Fmax and area.

And the area number is fucking enormous Summing the isolated leaves:

15,749 ALMs = 37.6% of the FPGA 25,123 registers 11 M10K 16 DSP

for the texture island alone.

I would not treat 37.6% as a future composed-console resource number; synthesis can change things during integration and several blocks are deliberately temporary. But the ratio 25k registers vs 11 M10Ks is incredibly diagnostic. The island is storing things in fabric that belong in RAM.

So this isn't “oh no, the final console needs 38% for textures.”

It's:

“Thank Christ we measured this before integrating it, because the prototype microarchitectures are very obviously not production hardware.”

RCP24 has a weird result RCP24 measured 68.46 MHz, but its data path itself is only 8.516 ns. The reported failure contains nearly 5.95 ns of clock skew into the output register.

The agent initially did the dumb thing: reran the same seed and got… 68.46 again. Then caught itself:

Quartus is deterministic at the same seed. I just repeated the same placement.

So run_block_fit now has a seed option, and RCP24 seeds 2 and 3 are part of the newly launched round. That's exactly correct methodology.

The repair/refit round is running now At 05:10 Zurich time the agent committed the Round-3 launch:

TEXJOIN v2 — work FIFO + registered outputs

AUX pipe — registered input boundary

TMU planner — narrowed widths

RCP24 — seeds 2 and 3

There is no new committed Fmax for those repaired versions yet. So the next update should be particularly interesting.

The planner's original 93.55 MHz has already produced one sane fix: the critical cone was doing 32-bit wrap/address arithmetic on quantities that physically need about 12–24 bits. It was narrowed while maintaining 357/357 exact addresses against the old TMU. Again: old Fmax, new RTL pending refit.

Zixx: QA actually did its fucking job There was another entire drama overnight.

The first independent QA looked at the previous D23 “fixed” version and said:

FAIL. NOT PUBLISHED.

Specifically, the claimed whole-body improvement didn't reproduce. The implementer claimed tail contribution dropped 82.5→64.6%; QA measured 80.2%, robustly ~77–81% across variants. The middle had technically moved 28 mm, but that was 0.7 pixels at 240p. So it was mathematically changed and visually basically unchanged.

Excellent catch.

Direction 24 then did the obvious artistic fix rather than gaming the metric:

plant the tail.

Stations 15–18 literally alias the resting stance, so the tail's spline path is now constant by construction. The movement budget moves forward into the body and the actual S grows from the planted rear. Beat-one change distribution becomes:

80.2 / 7.7 / 12.1% → 41.3 / 25.1 / 33.6%

tail / middle / head.

Then QA-2 independently passed it:

planted tail: PASS

visible S growth: PASS

whole-body movement: PASS

all seven Direction-23 criteria: PASS

silhouette motion 0.92%/frame median, ~3.5× smoother than the accepted balance clip

launch actually stronger: nose 2.42 px/frame vs 2.17

15/22 subjects byte-identical; exactly the seven spring consumers changed.

Current main is dbc0deec…:

“clean-rebuild probe re-run immediately before publication — PASS.”

But there is still no subsequent PUBLISHED commit. So right now Zixx is in the amusing state of being QA-green, clean-rebuild-green, explicitly immediately-before-publication… and apparently not published yet.

So today's headline is actually extremely valuable:

The 99.5 MHz renderer survived. The first texture prototypes did not.

That means we found the architecture wall before pretending the console was nearly integrated. And better still, the worst failures aren't mysterious “FPGA too small” problems. Three of the four worst paths have very concrete structural causes:

missing seam register → giant priority scan → rescale cone → fake RAM implemented as flops.

Those are fixable problems.

The first refit numbers after those fixes are now the numbers I care about most.

Alright, we need a big design doc in txt that fixes these. Go architect these things into the strategy and go save our ALMs and DSPs

Your servers crashed, please rearchitect like you've been doing and finish up with a txt

Done. The architecture brief is 2,671 lines / 75 KB:

Download the Zhaozhou texture-island production rearchitecture

Core ruling The answer is not to optimize the ten prototype leaves independently until their combined wreckage happens to fit.

The first island spent 15,749 ALMs, 25,123 registers, and 16 DSPs, with eight of ten blocks below the renderer’s 99.50 MHz best result. The especially damning number is 25,123 registers against only 11 M10Ks: queues, cache lines, tags, contexts, and results were being built primarily out of fabric.

The replacement changes ownership:

One central M10K-backed survivor/transaction store owns fragment state, sample state, ordering, and retirement exactly once. Every arithmetic or memory service carries compact tokens rather than building another ROB and copying the packet again.

That is the main ALM rescue.

The big architectural moves One central TEXJOIN/survivor store It allocates a token immediately after Early-Z, before reciprocal and perspective recovery. The production default is 32 slots. Frequently modified masks and generations remain in flops; UV numerators, sample results, context, world position, AUX state, and final results move into banked synchronous M10Ks.

RCP, PERSPUV, Mosaic, cache, palette, filter, AUX, and the material combiner may complete out of order. They return {slot, generation, subindex}. Only the central store retires in allocation order.

That deletes the same state being maintained repeatedly by TEXJOIN, PERSPUV, TMU, AUX, and the response dispatcher.

One unified 8 KiB read-only cache The earlier four-lane cache choice is explicitly superseded by new physical evidence. The production cache is:

256 sets × 2 ways

512 lines × 16 bytes = 8 KiB

eight data M10Ks plus two tag M10Ks

two line lookups per clock using the memories’ dual ports

deterministic round-robin replacement

no dirty bits, writeback, coherence, PLRU, or generic CPU-cache nonsense

A four-tap sample is converted into one to four unique line jobs. The same physical line is stored once, not copied into several lane caches. Even the pessimistic three-sample envelope with every bilinear footprint hitting four lines generates about 1.23 million line jobs, versus 2.67 million line-job capacity in the 1,333,333-clock design window.

This should turn the cache from 5,634 ALMs / 10,812 registers / 3 M10Ks into roughly 900–1,400 ALMs / under 1,500 registers / 10 M10Ks. That is a target, not a guaranteed fit, but the storage shape now matches the hardware. The current contract itself confirms that four complete lane caches and flopped tags were implementation choices rather than pixel law.

Scheduled narrow multiplication The document does not blindly move multipliers into ALMs. It defines explicit width-proved kernels and bake-offs:

RCP: two high-level multiplier lanes, ideally one 27×27 DSP apiece plus explicit five-bit cross terms. Four dependent jobs per reciprocal and two launches per clock gives theoretical reciprocal II=2, about 666k reciprocals inside the design window. Objective: 2 DSP, hard ceiling 4.

Perspective: U and V are physically parallel. Each uses one 27×24 DSP plus a signed five-bit cross term, followed by a split coarse/fine rescale pipeline. Objective: 2 DSP total, one UV pair per clock.

Bilinear: the exact three-product factorization is retained, but one 18×9 DSP is scheduled across its three micro-operations. Known demand is 180k channel jobs; capacity becomes about 444k. That replaces 3 DSP with 1 without endangering the workload.

Mosaic: X and Y constant products are interleaved through one narrow multiplier, giving one decision per two clocks and reducing 4 DSP to 1. A zero-DSP CSD fallback is specified if the refreshed whole-console DSP census demands it.

The current code and contracts confirm the arithmetic that must remain exact: four reciprocal multiply jobs, the perspective rescale, the three-product bilerp, and Mosaic’s wrapping 32-bit hash.

AUX loses half its arithmetic The prototype pipelines U and V in parallel, but AUX does not need one completed request every clock.

The new service performs one axis per clock, so one AUX request takes two issue clocks. That still supports roughly 666k AUX requests in the design window against a fragment envelope of at most 320k.

It also removes:

the separate side table;

the wide ordered return FIFO;

four envelope endpoints from each transaction;

duplicated U/V divider hardware.

The envelope moves into a resident AUX binding. The central join handles out-of-order returns by token.

Static texture interpretation leaves the hot path The per-sample planner no longer receives and decodes a raw 32-bit mode word plus base and palette addresses.

A sealed binding table supplies:

format and filter;

wrap modes;

dimensions and maximum mip;

palette slot/generation;

content generation;

response class.

A separate 64-binding × 16-level table stores exact mip bases for about four M10Ks. This removes REP4/base reconstruction from the hot planner while preserving every sampling law.

Wide response dispatch disappears zhao_texture_rsp_dispatch currently spends 806 ALMs and 1,432 registers copying wide responses through a raw FIFO and three class FIFOs.

The new cache writes tap data once into a token-indexed sample record. CLUT, nearest, and bilerp queues carry only the compact sample key. One completion arbiter writes one final sample per clock.

The behavioral decoupling principle survives; the wide implementation does not.

The material combiner becomes real and costs zero DSP All eight now-frozen recipes are included, including the missing three-sample terrain recipes. The combiner is its own tokenized pipeline rather than a combinational case on TEXJOIN retirement.

Three explicit shift/add 8-bit channel multipliers run in parallel:

two-sample recipe: one pass;

three-sample recipe: two passes;

maximum capacity: about 666k fragments/frame;

DSP cost: zero.

The arithmetic is copied from the newly frozen material law rather than invented: unit_mul8, modulate2x8, lerp8, saturated add, mask, detail-light, and detail-mask.

Quantitative target The production island objectives are:

Resource Prototype leaves Objective Hard gate ALMs 15,749 5,600–6,500 8,500 Registers 25,123 ≤7,000 9,000 M10Ks 11 ≈48 64 DSPs 16 6 10 On the provisional device, 48 M10Ks are about 8.7% of the available 553, while six DSPs are about 5.4% of 112. This is exactly the exchange we want: spend abundant block memory to recover scarce routing fabric and multiplier structures.

Those are engineering gates, not claimed Quartus results. The brief specifies what happens if a block misses rather than pretending estimates are measurements.

It also corrects the fit methodology The document rejects treating 829-virtual-pin raw leaf fits as final physical truth. New low-pin wrappers place real registered or M10K packet sources and sinks around each cluster. Those registers remain real architecture seams; nothing is hidden with false constraints.

The required fit staircase is:

survivor store with randomized stub services;

RCP + perspective frontend;

binding + planner;

cache;

gather + palette + nearest + bilerp;

AUX;

combiner;

complete texture island;

Early-Z → island → fragment composition;

full console.

Every promotion has explicit Fmax, ALM, register, M10K, DSP, three-seed, RAM-inference, differential, and mutation gates.

The current hardware branch remains at 1c0a7f4; no repaired Round-3 fit result landed while I was writing. The running set is still the work-FIFO TEXJOIN, registered-input AUX, narrowed planner, and RCP seeds 2 and 3.

This is the brief I would hand the agent as the new governing texture strategy.

this is bros's solution to the bad fit

Done. The full architecture brief is 1,704 lines / 57.6 KB and includes the corrected buildability ledger, binding rulings, recent-RTL review, complete 8 km terrain-world decisions, exact integration gates, and the revised build order.

Download the full Zhaozhou buildability and architecture brief

Main verdict The submitted audit was useful, but not safe to ratify unchanged. Several of its supposed owner questions had already been answered by the binding August 31 rulings:

Depth is already fixed as three named profiles, with generated and proved scales. You do not need to choose wmin, wmax, or scale again.

276,480 already means pre-Early-Z covered fragments for Z60 at 3× overdraw. It is not a TMU sample count.

Particle overflow, spawn ordering, collision sources and responses, recipe vocabulary, update order, and representation ladder are already substantially frozen.

TWOD.SPRITE is already HUD-only and shares the primary TMU; TWOD.PLANE is already the restricted world-plane engine; post-processing order is already frozen.

MEASURE.HISTOGRAM should remain deliberately refused.

Conversely, the claim that zero contracts remain accidentally unwritten is false. SW.STREAM, the software side that must prepare and stream the island world, still contains substantive TODO sections. And the August 31 architecture requires an external-SDRAM GEOM.PARAMBUF, but no corresponding ledger entry, contract, reference implementation, or RTL currently exists.

Rulings made Depth and fragment budgets Depth remains:

WORLD_LONG: 1 m–16,384 m, scale 2^40

WORLD_STANDARD: 0.5 m–8,192 m, scale 2^39

CLOSE: 0.25 m–2,048 m, scale 2^38

SetView.flags[1:0] already carries the profile. wmax is a depth clamp, not a far clipping plane. The old open-depth report should be marked superseded.

The canonical workload distinction is now:

320,000 pre-Early-Z fragments/frame as the cross-mode design target.

Post-Z survivors are a separate measured trace.

Material samples are a separate measured trace.

AUX requests are a separate measured trace.

1,333,333 clocks/frame is the performance-design budget.

1,666,667 clocks/frame is the late-frame fault boundary, not the target.

Particle path The particle behaviours were already specified. The true missing law was numeric interpretation. I ruled:

Position: signed S9.8 metres relative to a population origin — 1/256 m resolution and roughly ±512 m local range.

Velocity: signed S2.8 metres per 60 Hz tick — roughly ±240 m/s.

Size: unsigned U2.4, interpreted as a relative radius scale.

Spin: U0.6 turns.

Population origins are fx16 and may be deterministically rebased between ticks.

Existing survivors outrank children; never evict the oldest survivor to admit a spawn.

Terrain and simple planes are the FPGA collision sources; creature/gameplay collision remains HPS-owned.

This is capture-visible numeric law, so the brief calls for QFMT_VERSION 2 → 3 and a particle format version.

2D and post-processing The plane engine receives two explicit roles:

BACKDROP: no scene-depth interaction.

ATMOSPHERE: bounded alpha/additive overlay with optional world anchoring.

Water or lava that must genuinely intersect world geometry uses ordinary triangles. The restricted plane engine must not turn into a second general TMU.

POST.GATHER’s current memory arithmetic is internally inconsistent: the stated wide accumulators do not fit its approximately 31 KB / ≤10-M10K claim. I ruled a tile-local wide accumulator followed by compact global quarter-resolution storage:

RGB565 glow

signed 8-bit X displacement, bounded ±8 pixels

signed 8-bit Y displacement, bounded ±4 scanlines

one-bit ink mask

That makes the real global storage approximately 30 M10Ks in Duo, not ≤10.

POST.COMPOSITE should be a bounded line-streaming pipeline with a displacement line ring, not five complete framebuffer rereads. Generic exact 65,536-entry RGB565 grading tables are rejected for v1; use generated per-channel curves plus a small exact matrix/bias transform.

Binner and GEOM.PARAMBUF I reject the new suggestion to solve binner capacity merely by choosing 8/16/32/64 full-mesh creatures and growing the on-chip arena. The newer LOD measurements are genuinely good news, but they do not supersede the binding external-parameter-buffer decision.

The ruling is:

Build GEOM.PARAMBUF in local SDRAM under ENGINE1.

Keep only directories, active tails, prefetch FIFOs, and opportunistic caches on chip.

Guarantee 32 ordinary full-mesh creatures machine-wide, with at least 16 available to either active Duo view.

Treat the giant separately with a dedicated 32,768-reference reserve; it must not consume the ordinary creature quota.

Unexpected hard overflow repeats the previous complete frame rather than publishing an arbitrary missing tail.

Three-sample materials Three-sample terrain is a real shipping capability, but not a promise that every visible terrain fragment always receives all three samples.

The quality ladder is:

base + detail + light/mask

base + detail

base only

Gouraud/glint fallback where permitted

The governor chooses the level before sealing the frame. The 1,094,600-sample profile remains an upper-envelope stress gate, while real scheduling must use post-Early-Z material-request traces.

I also froze exact native combiner operations for passthrough, modulate, modulate-2×, lerp, saturated add, mask, and terrain detail/light combinations, all using the machine’s existing unit8 /256 arithmetic. The current “sample 0 plus combiner_unfrozen” implementation is useful scaffolding but is not a finished material path.

Problems in today’s new texture RTL The recent work is conceptually strong, but it is not yet ready to integrate merely because the directed leaf tests are green.

PERSPUV The reported 415,627 fragments/frame and 50% headroom assumes one recovered UV pair per fragment. That is not sufficient evidence for the ratified three-sample architecture. Three independent UV sets would require:

1,094,600 × 2 = 2,189,200 product launches/frame,

which cannot pass through one one-product-per-clock lane inside the 1,333,333-clock design budget.

The production ruling is therefore two parallel product paths—U and V concurrently—yielding one perspective pair per clock, unless traces prove that secondary terrain samples always share an already-recovered coordinate pair. The existing scheduled reciprocal remains sensible.

TEXJOIN v2 The current implementation has several concrete problems:

It allocates after perspective, even though transaction ownership should begin immediately after Early-Z.

It uses a two-bit generation, wrapping after four slot reuses; production uses eight bits.

free_cnt_q is independently decremented on acceptance and incremented on retirement in the same always_ff; same-cycle acceptance and retirement lose one update. This is the exact bookkeeping fault that the PERSPUV tests already found in another block.

Its tests do not currently exercise that steady-state same-cycle case.

It performs broad combinational scans across all entries to choose TMU and AUX work.

TMU and AUX return paths are declared permanently ready without skid capacity or duplicate-return protection.

A zero-sample fragment can expose uninitialised sample-0 storage.

Its output is combinational from the table head rather than registered.

The material combiner remains deliberately unfinished.

The brief specifies a production replacement while keeping this implementation as a useful architectural prototype.

TMU planner, palette and cache The elastic TMU planner’s corrected 0/357 address comparison is good, but it currently proves only the non-CLUT planning subset. Before adoption it needs the complete inherited texture-format suite, including CLUT behaviour, palette residency, mip edges, wrap modes, stalls, repeated external IDs, and out-of-order completion.

The resident palette needs a transactional BEGIN / WRITE / END load protocol, CRC, eight-bit generations, and same-slot lookup exclusion while loading.

The cache’s same-line fill multicast is an excellent correction, but the new source still performs combinational array reads and compares rather than the required:

synchronous RAM read → fabric capture → registered classify

That may prevent the intended M10K implementation and recreate the RAM-output timing problem this rearchitecture was supposed to remove. The FIFO pointer widths are also effectively hard-coded around the present depth.

Terrain residency The direct-mapped 1,024-slot residency prototype should not be integrated as production architecture.

It lacks island identity in its key, has a deterministic 2,048 m collision period, has only four-bit generations, does not reserve dirty victims until writeback finishes, has no pin/in-flight protection, cannot unload without global reset, and its metadata access pattern is not a proper synchronous-M10K directory. The current source has repaired one same-cycle loader-finish hazard, but the broader transaction problem remains.

The brief rules a TERRAIN.RESIDENCY_V2:

256 sets × four ways

full {resource_epoch, island_id, ix, iz} tags

deterministic round-robin victim selection

eight-bit generation

explicit EMPTY / LOADING / RESIDENT_CLEAN / RESIDENT_DIRTY / WRITEBACK states

in-flight pin counts

dirty writeback acknowledgment before reuse

synchronous metadata RAM

explicit epoch teardown

8 km terrain-world rulings The file provides a complete memory and command architecture rather than leaving ten more questions for a later brief. Major decisions include:

Full patch identity includes resource_epoch, island_id, patch_ix, and patch_iz.

Static authored pages use CPU-canonical scar/breach state.

FPGA writes back mutable surface-sheet layer F; B/D remain CPU canonical.

Dynamic composed-height cache remains 256 patches, while static terrain can bypass it.

Prefetch policy: current visible set + one-patch ring + bounded 30-frame velocity prediction.

Initial prefetch cap: 32 complete pages/frame, treated as provisional until board bandwidth evidence.

Coarse height mips are generated on the FPGA by exact 33→17→9 sample selection, not averaging.

Local SDRAM bank 2 receives a concrete terrain-world map, while ENGINE1 remains render-critical and a new best-effort TERRAIN_BUILD client handles page loading and writeback.

Two additive terrain command records are specified: epoch/control and deterministic terrain-set submission.

A dense 8×8 km plate at 2 m pitch still cannot fit in 128 MB; the product target remains a sparse streamed world with 1,024 resident patches.

Immediate build order Fix the block-fit source manifests. The most recent apparent Quartus failure was not a failed design: the generated QSF omitted the new module and its dependencies because run_block_fit -Module does not add sources by itself.

Repair and fit the texture leaves individually.

Build one real texture-survivor composition: Early-Z → token allocation → reciprocal/perspective → multi-sample issue → cache/TMU/palette → AUX → exact combiner → registered retirement.

Meet the staged physical gates: approximately 150 MHz leaves, 120–125 MHz islands, 115–120 MHz texture-survivor composition.

Build and integrate external GEOM.PARAMBUF.

Only then build terrain-world streaming and residency v2.

Particles, planes, gather and post follow after the conventional full renderer is physically credible.

The hierarchy remains:

105 MHz full-composition acceptance floor

110 MHz architecture objective

115 MHz stretch/headroom

100 MHz product clock

The central conclusion is:

Zhaozhou is not blocked by dozens of unanswered game-design questions. It is blocked by a much smaller set of concrete numeric, storage, transaction and physical-composition problems—and several of today’s “finished leaves” still need one more architecture pass before they are allowed into the machine.

The text file is deliberately marked proposed owner rulings. Committing or otherwise adopting it would make those decisions binding; I did not modify the repository.

ZHAOZHOU_BUILDABILITY_RULINGS_2026-09-02.txt Dokument

Big fucking news. The texture fit finally answered the question.

Hardware: the first texture architecture does not hold 100 MHz All 10 texture-island blocks have now been physically fitted. The answer in the agent's own report is literally:

“No.” “It needs more reengineering.”

The one-seed leaf results are:

Block Fmax AUX pipe 54.95 MHz TEXJOIN v2 61.66 PERSPUV svc 62.67 RCP24 svc 68.46 Cache pipe 81.06 Mosaic 86.63 TMU planner 93.55 Bilerp lane 99.69 Palette resident 104.42 Response dispatcher 110.90 So: 8/10 are slower than the existing 99.50 MHz renderer, and none reaches the 120–125 MHz island target, never mind 150 MHz leaf headroom.

But this is not “the console is 54.95 MHz.” These are isolated virtual-I/O leaf fits, not the composed machine. Several of the worst numbers are already known to be measuring architecture we have explicitly ordered him to replace. Composition generally makes things worse, but these exact numbers are not the future architecture's numbers.

And this is why making him FIT THE DAMN THING NOW was the right call.

The really useful part: TimeQuest named the actual defects AUX 54.95: its worst path literally starts at an input pin and runs through clamp/normalisation into the divider. The “pipelined” block had no registered input seam. He's already inserted one: A0 captures subtract/shift, A0b does clamp/mux. II stays 1. That 54.95 is now explicitly a before number.

TEXJOIN 61.66: our feared 16×3 combinational issue scan is absolutely real. The path is 13 logic levels / ~15.7 ns, mostly routing. He's now replaced the scan with a 64-entry work FIFO, preserving oldest-first issue order while removing the giant priority search. TMU/AUX/retirement outputs are registered too, and zero-sample retirement was fixed with a test that actually fails on the old implementation. 33 checks pass. Not refitted yet.

PERSPUV 62.67: product register → variable rescale → saturation → entry storage is one enormous ~15 ns cone. This is exactly the other half of what the brief told him to repair. And remember: this is before the production two-parallel-product-lane redesign, so PERSPUV clearly needs real surgery rather than merely another copy.

Cache 81.06: this one is spectacular confirmation of our review. It uses 5,634 ALMs and 10,812 registers with only 3 M10Ks. In other words: yes, the supposed cache arrays really did become a gigantic sea of flip-flops because of the combinational read structure. The C0→C4 synchronous RAM/capture/classify architecture isn't optional; it's the path to fix both Fmax and area.

And the area number is fucking enormous Summing the isolated leaves:

15,749 ALMs = 37.6% of the FPGA 25,123 registers 11 M10K 16 DSP

for the texture island alone.

I would not treat 37.6% as a future composed-console resource number; synthesis can change things during integration and several blocks are deliberately temporary. But the ratio 25k registers vs 11 M10Ks is incredibly diagnostic. The island is storing things in fabric that belong in RAM.

So this isn't “oh no, the final console needs 38% for textures.”

It's:

“Thank Christ we measured this before integrating it, because the prototype microarchitectures are very obviously not production hardware.”

RCP24 has a weird result RCP24 measured 68.46 MHz, but its data path itself is only 8.516 ns. The reported failure contains nearly 5.95 ns of clock skew into the output register.

The agent initially did the dumb thing: reran the same seed and got… 68.46 again. Then caught itself:

Quartus is deterministic at the same seed. I just repeated the same placement.

So run_block_fit now has a seed option, and RCP24 seeds 2 and 3 are part of the newly launched round. That's exactly correct methodology.

The repair/refit round is running now At 05:10 Zurich time the agent committed the Round-3 launch:

TEXJOIN v2 — work FIFO + registered outputs

AUX pipe — registered input boundary

TMU planner — narrowed widths

RCP24 — seeds 2 and 3

There is no new committed Fmax for those repaired versions yet. So the next update should be particularly interesting.

The planner's original 93.55 MHz has already produced one sane fix: the critical cone was doing 32-bit wrap/address arithmetic on quantities that physically need about 12–24 bits. It was narrowed while maintaining 357/357 exact addresses against the old TMU. Again: old Fmax, new RTL pending refit.

Zixx: QA actually did its fucking job There was another entire drama overnight.

The first independent QA looked at the previous D23 “fixed” version and said:

FAIL. NOT PUBLISHED.

Specifically, the claimed whole-body improvement didn't reproduce. The implementer claimed tail contribution dropped 82.5→64.6%; QA measured 80.2%, robustly ~77–81% across variants. The middle had technically moved 28 mm, but that was 0.7 pixels at 240p. So it was mathematically changed and visually basically unchanged.

Excellent catch.

Direction 24 then did the obvious artistic fix rather than gaming the metric:

plant the tail.

Stations 15–18 literally alias the resting stance, so the tail's spline path is now constant by construction. The movement budget moves forward into the body and the actual S grows from the planted rear. Beat-one change distribution becomes:

80.2 / 7.7 / 12.1% → 41.3 / 25.1 / 33.6%

tail / middle / head.

Then QA-2 independently passed it:

planted tail: PASS

visible S growth: PASS

whole-body movement: PASS

all seven Direction-23 criteria: PASS

silhouette motion 0.92%/frame median, ~3.5× smoother than the accepted balance clip

launch actually stronger: nose 2.42 px/frame vs 2.17

15/22 subjects byte-identical; exactly the seven spring consumers changed.

Current main is dbc0deec…:

“clean-rebuild probe re-run immediately before publication — PASS.”

But there is still no subsequent PUBLISHED commit. So right now Zixx is in the amusing state of being QA-green, clean-rebuild-green, explicitly immediately-before-publication… and apparently not published yet.

So today's headline is actually extremely valuable:

The 99.5 MHz renderer survived. The first texture prototypes did not.

That means we found the architecture wall before pretending the console was nearly integrated. And better still, the worst failures aren't mysterious “FPGA too small” problems. Three of the four worst paths have very concrete structural causes:

missing seam register → giant priority scan → rescale cone → fake RAM implemented as flops.

Those are fixable problems.

The first refit numbers after those fixes are now the numbers I care about most.

Alright, we need a big design doc in txt that fixes these. Go architect these things into the strategy and go save our ALMs and DSPs

Your servers crashed, please rearchitect like you've been doing and finish up with a txt

Done. The architecture brief is 2,671 lines / 75 KB:

Download the Zhaozhou texture-island production rearchitecture

Core ruling The answer is not to optimize the ten prototype leaves independently until their combined wreckage happens to fit.

The first island spent 15,749 ALMs, 25,123 registers, and 16 DSPs, with eight of ten blocks below the renderer’s 99.50 MHz best result. The especially damning number is 25,123 registers against only 11 M10Ks: queues, cache lines, tags, contexts, and results were being built primarily out of fabric.

The replacement changes ownership:

One central M10K-backed survivor/transaction store owns fragment state, sample state, ordering, and retirement exactly once. Every arithmetic or memory service carries compact tokens rather than building another ROB and copying the packet again.

That is the main ALM rescue.

The big architectural moves One central TEXJOIN/survivor store It allocates a token immediately after Early-Z, before reciprocal and perspective recovery. The production default is 32 slots. Frequently modified masks and generations remain in flops; UV numerators, sample results, context, world position, AUX state, and final results move into banked synchronous M10Ks.

RCP, PERSPUV, Mosaic, cache, palette, filter, AUX, and the material combiner may complete out of order. They return {slot, generation, subindex}. Only the central store retires in allocation order.

That deletes the same state being maintained repeatedly by TEXJOIN, PERSPUV, TMU, AUX, and the response dispatcher.

One unified 8 KiB read-only cache The earlier four-lane cache choice is explicitly superseded by new physical evidence. The production cache is:

256 sets × 2 ways

512 lines × 16 bytes = 8 KiB

eight data M10Ks plus two tag M10Ks

two line lookups per clock using the memories’ dual ports

deterministic round-robin replacement

no dirty bits, writeback, coherence, PLRU, or generic CPU-cache nonsense

A four-tap sample is converted into one to four unique line jobs. The same physical line is stored once, not copied into several lane caches. Even the pessimistic three-sample envelope with every bilinear footprint hitting four lines generates about 1.23 million line jobs, versus 2.67 million line-job capacity in the 1,333,333-clock design window.

This should turn the cache from 5,634 ALMs / 10,812 registers / 3 M10Ks into roughly 900–1,400 ALMs / under 1,500 registers / 10 M10Ks. That is a target, not a guaranteed fit, but the storage shape now matches the hardware. The current contract itself confirms that four complete lane caches and flopped tags were implementation choices rather than pixel law.

Scheduled narrow multiplication The document does not blindly move multipliers into ALMs. It defines explicit width-proved kernels and bake-offs:

RCP: two high-level multiplier lanes, ideally one 27×27 DSP apiece plus explicit five-bit cross terms. Four dependent jobs per reciprocal and two launches per clock gives theoretical reciprocal II=2, about 666k reciprocals inside the design window. Objective: 2 DSP, hard ceiling 4.

Perspective: U and V are physically parallel. Each uses one 27×24 DSP plus a signed five-bit cross term, followed by a split coarse/fine rescale pipeline. Objective: 2 DSP total, one UV pair per clock.

Bilinear: the exact three-product factorization is retained, but one 18×9 DSP is scheduled across its three micro-operations. Known demand is 180k channel jobs; capacity becomes about 444k. That replaces 3 DSP with 1 without endangering the workload.

Mosaic: X and Y constant products are interleaved through one narrow multiplier, giving one decision per two clocks and reducing 4 DSP to 1. A zero-DSP CSD fallback is specified if the refreshed whole-console DSP census demands it.

The current code and contracts confirm the arithmetic that must remain exact: four reciprocal multiply jobs, the perspective rescale, the three-product bilerp, and Mosaic’s wrapping 32-bit hash.

AUX loses half its arithmetic The prototype pipelines U and V in parallel, but AUX does not need one completed request every clock.

The new service performs one axis per clock, so one AUX request takes two issue clocks. That still supports roughly 666k AUX requests in the design window against a fragment envelope of at most 320k.

It also removes:

the separate side table;

the wide ordered return FIFO;

four envelope endpoints from each transaction;

duplicated U/V divider hardware.

The envelope moves into a resident AUX binding. The central join handles out-of-order returns by token.

Static texture interpretation leaves the hot path The per-sample planner no longer receives and decodes a raw 32-bit mode word plus base and palette addresses.

A sealed binding table supplies:

format and filter;

wrap modes;

dimensions and maximum mip;

palette slot/generation;

content generation;

response class.

A separate 64-binding × 16-level table stores exact mip bases for about four M10Ks. This removes REP4/base reconstruction from the hot planner while preserving every sampling law.

Wide response dispatch disappears zhao_texture_rsp_dispatch currently spends 806 ALMs and 1,432 registers copying wide responses through a raw FIFO and three class FIFOs.

The new cache writes tap data once into a token-indexed sample record. CLUT, nearest, and bilerp queues carry only the compact sample key. One completion arbiter writes one final sample per clock.

The behavioral decoupling principle survives; the wide implementation does not.

The material combiner becomes real and costs zero DSP All eight now-frozen recipes are included, including the missing three-sample terrain recipes. The combiner is its own tokenized pipeline rather than a combinational case on TEXJOIN retirement.

Three explicit shift/add 8-bit channel multipliers run in parallel:

two-sample recipe: one pass;

three-sample recipe: two passes;

maximum capacity: about 666k fragments/frame;

DSP cost: zero.

The arithmetic is copied from the newly frozen material law rather than invented: unit_mul8, modulate2x8, lerp8, saturated add, mask, detail-light, and detail-mask.

Quantitative target The production island objectives are:

Resource Prototype leaves Objective Hard gate ALMs 15,749 5,600–6,500 8,500 Registers 25,123 ≤7,000 9,000 M10Ks 11 ≈48 64 DSPs 16 6 10 On the provisional device, 48 M10Ks are about 8.7% of the available 553, while six DSPs are about 5.4% of 112. This is exactly the exchange we want: spend abundant block memory to recover scarce routing fabric and multiplier structures.

Those are engineering gates, not claimed Quartus results. The brief specifies what happens if a block misses rather than pretending estimates are measurements.

It also corrects the fit methodology The document rejects treating 829-virtual-pin raw leaf fits as final physical truth. New low-pin wrappers place real registered or M10K packet sources and sinks around each cluster. Those registers remain real architecture seams; nothing is hidden with false constraints.

The required fit staircase is:

survivor store with randomized stub services;

RCP + perspective frontend;

binding + planner;

cache;

gather + palette + nearest + bilerp;

AUX;

combiner;

complete texture island;

Early-Z → island → fragment composition;

full console.

Every promotion has explicit Fmax, ALM, register, M10K, DSP, three-seed, RAM-inference, differential, and mutation gates.

The current hardware branch remains at 1c0a7f4; no repaired Round-3 fit result landed while I was writing. The running set is still the work-FIFO TEXJOIN, registered-input AUX, narrowed planner, and RCP seeds 2 and 3.

This is the brief I would hand the agent as the new governing texture strategy.

ZHAOZHOU_TEXTURE_ISLAND_PRODUCTION_REARCHITECTURE_2026-09-03.txt Dokument

Keine ausgewählt Bibliothek / ZHAOZHOU_TEXTURE_ISLAND_PRODUCTION_REARCHITECTURE_2026-09-03.txt ZHAOZHOU_TEXTU…2026-09-03.txt

ZHAOZHOU TEXTURE-SURVIVOR ISLAND PRODUCTION REARCHITECTURE, ALM RECOVERY, AND DSP RECOVERY PLAN
Date: 2026-09-03 Primary source snapshot: Fabulu/zhaozhou, branch zixxtrixx-v8-closeout Primary source commit: 1c0a7f44ebb82cb3fd5dd74bd182a5385dff3f1e Physical evidence source: reports/TEXTURE-ISLAND-FIT.md Earlier governing direction: reports/REARCHITECTUREADVICE.md reports/OWNER-RULINGS-BUILDABILITY-20260902.md reports/MATERIAL_ARCHITECTURE.md reports/RENDERER_ARCHITECTURE.md design/budgets/workloads.yml design/budgets/dsp.md

STATUS
PROPOSED OWNER ARCHITECTURE.

This document is written to be handed directly to the implementation agent. It does not modify the repository. It becomes binding when the owner adopts or commits it.

This document supersedes the implementation shape, but not the arithmetic, pixel, ordering, material, or fault laws, of the first texture-island leaf prototypes. Those prototypes remain executable specifications and mutation targets until the production island passes its composed differential gates.

The first implementation was valuable. It proved the functional ideas and then the fit proved that its physical organization is not shippable. The response is not to discard the exact arithmetic. The response is to stop storing the same transaction in several places, stop scanning arrays to discover work, stop building memories out of flip-flops, and stop paying a DSP for every operation that happened to be written with *.

EXECUTIVE RULING
The 99.50 MHz reduced renderer remains provisionally frozen.

The first texture-island leaf set does NOT survive at the renderer clock:

zhao_texture_aux_pipe 54.95 MHz zhao_raster_texjoin_v2 61.66 MHz zhao_raster_perspuv_svc 62.67 MHz zhao_raster_rcp24_svc 68.46 MHz zhao_texture_cache_pipe 81.06 MHz zhao_texture_mosaic 86.63 MHz zhao_texture_tmu_plan 93.55 MHz zhao_texture_bilerp_lane 99.69 MHz zhao_texture_palette_res 104.42 MHz zhao_texture_rsp_dispatch 110.90 MHz

The isolated sum was:

15,749 ALMs 25,123 registers 11 M10Ks 16 DSP blocks

