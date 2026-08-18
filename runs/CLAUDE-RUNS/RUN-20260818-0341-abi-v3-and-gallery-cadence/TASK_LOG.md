# RUN-20260818-0341 — ABI v3 completion, gallery cadence, shell characterization

Owner asks handled in this run: finish the red CI, slow the quadruped's legs,
make the moving-sun smears sun-sized, and get real Quartus evidence for the
composed shell. One agent policy observed: no subagents were used.

## State inherited

`main` was red at `e3f316b`. Three CI jobs failed:

| Job | Cause |
|---|---|
| npm tooling | root `package.json` listed a `compiler` workspace the lock file no longer had, so `npm ci` refused |
| format + static analysis | LLVM-15 formatting drift, then a cppcheck `objectIndex` finding |
| cmake + ctest (fast) | six C++ tests left behind by the ABI v2 -> v3 bump |

Also inherited: an uncommitted QSF hunk, and 18 directories of raw reel frames
at the repo root (~300 MB) which this run created by invoking `zhao-reel.exe`
with no output argument, where it defaults to `.`. Deleted; nothing tracked was
touched.

## Work

### `9f7a668` — hardware-only CI gates
Reverted the root manifest to the three tooling workspaces so `npm ci` matches
the lock, and applied pinned LLVM-15 formatting. npm lane green: 75 tests.

### `abf30b8` — four fitter processors
Quartus holds fit results invariant to processor count; the seed pins
reproducibility. Kept the inherited hunk deliberately rather than discarding it.

### `d8e7c88` — ABI v3 through the C++ lane
`5cebbcd` bumped the wire to v3 and QFMT to 2 on generator evidence alone. Six
tests were left behind, each wrong for its own reason:

- `stub_top` asserted v3 is a reject; it is now the accepted wire. Both reject
  cases now derive from `ZHAO_ABI_VERSION` so they cannot go stale again.
- `cmd_dma` built every frame header with a literal `2`, so a v3 decoder
  correctly refused all of them and every status collapsed to BAD_ABI_VERSION.
- `fixp` pinned QFMT 1. Amendment C1 added the quat16 lane rather than changing
  a law this file covers, so the checks are unchanged; quat16 is covered by
  `tests/geometry/creature_core.cpp`.
- `tables_tri` hardcoded the marker string; derived from the header now.
- `shell_golden`: the three 10-frame mode goldens were not regenerated with
  `duo_markers.zcap`. Verified at byte level before accepting: only the abi-gen
  provenance digest, `abi_version` `0x0002 -> 0x0003`, and the dependent
  header/section CRCs move. No scene drift.

### `5755d5c` — gallery cadence and smear scale
Quadruped: the 16-key stride ran four keys per frame while the body crept
0.022 m, so a full gait cycle covered 8.8 cm and a one-metre creature scrambled
in place. The clip clock is back on the sim clock at one tick per frame, so a
cycle is 32 frames and lays down 0.70 m, about one body length.

Suns: `ghost_r_px` was hand-authored into a 9..15 px band while the suns span
8..42 px, which is why every sun left the same footprint. Each subject now
smears at its own visible radius. Overlap costs no palette because the
reconstruction plane is six-bit: one class-ramp lookup yields at most 64 trail
colours however many falloffs a pixel sums. Every star sequence still clears
256; noctis-flare at 248 is the ceiling case.

### `4402bc5` — cppcheck objectIndex
`(&m.m00)[i]` walked off the first member of `ZhMat4fx`. The ABI
static_asserts pin the layout so it worked, but it is UB and cppcheck 2.13 on
the Linux runner is right to reject it. All sixteen members are named now. No
suppression was added.

## Evidence

- `reel --check`: all 18 sequence CRCs match
- fast CTest in `build-verify`: 86/87 observed passing in one run, plus
  `shell_golden_replay` verified directly at 756 checks after that run was
  killed part-way. Every fast test is therefore accounted for green.
- `cmd_dma_directed` 8894 checks, `cmd_dma_random` 1482, `stub_top` 46,
  `fixp` 29385050, `tables_tri` OK, `shell_golden` 756

## Interruptions

Three background jobs (Quartus elaboration, the fast CTest sweep, the GIF
encode) were killed externally part-way. Nothing was lost: CTest was resumed by
running its one outstanding test directly, and the GIF encode was restarted from
a clean state. The GIF set had been left half-new and half-a-day-old, which is
exactly the kind of mixed gallery that must never be deployed, so no deploy
happened until the encode completed in full.

## Gallery deploy

All 18 GIFs regenerated from one reel build, palette-exact, each verified by
decoding the shipped GIF back and comparing every frame byte for byte. Assembled
through `assemble.py` and deployed with `deploy.ps1`, so both copycheck gates
ran. Wrangler was never invoked by hand.

Deployment: `https://ff75be7a.zhaozhou.pages.dev` -> `https://zhaozhou.pages.dev`

The gallery was deliberately NOT deployed while a kill had left it half-new and
half-a-day-old. A mixed set would have published a page whose suns disagreed
with each other about their own trail law.

## Maturity

No hardware maturity is claimed here. Quartus analysis/elaboration of
`zhao_shell_top` is running against the provisional `5CSEBA6U23I7` target. That
target is a capacity and timing characterization only. It is not board truth,
not a programmed device, and not integration. No `HARDWARE_PROVEN` claim is made
or implied.

## Docket (owner, 2026-08-18)

### Open: flare-occlusion shows no real occlusion

Owner: "the fade the original lacked - both old and new doesn't have a real
effect, it's just a transparent stripe over the image with a white dot moving
left and right. I don't think that's the intended effect."

The `flare-occlusion` subject is supposed to show a sun passing BEHIND island
terrain, with the flare chain fading out as the disc is occluded and recovering
as it clears. What ships reads as a flat transparent stripe with a white dot
sliding left and right, which is not that.

Where to look:
- `tools/reel/zhao_reel.cpp`, the `celestial` case for flare-occlusion (it sets
  `L.probe_x` / `L.probe_y` and a flare mode; check the occluder is real
  geometry and not a painted band)
- `reference/src/zsky/star_flare.cpp` and the occlusion probe path: confirm the
  probe actually samples the depth/terrain buffer at the disc, and that the
  documented 15-frame fade grades the whole flare chain rather than a single
  alpha over the frame
- the subject note claims "Sun crosses behind island with 15-frame fade" — if
  the island is not in the scene at all, the note is the lie and the scene
  needs the island, not a stripe

Fix, then regenerate the reel, inspect DECODED frames visually rather than
trusting the CRC, re-pin `expect_seq_crc`, re-encode the GIF, and deploy through
`zhaozhou-site\deploy.ps1`. Do not deploy a mixed gallery.

#### Diagnosis (read before fixing)

The owner is right, and it is not a bug. The subject is authored to be a stripe
and a dot. In `cel_hook` case 4:

    L.disc_r_px = 0;  // <1.5 px projected: glint rung
    L.halo_r_px = 0;

There is no disc and no halo. What renders is the far-glint point plus the
b=7 anamorphic streak, so "a transparent stripe with a white dot moving left
and right" is a literal description of what the code asks for. The occlusion
probe and the 15-frame fade DO work; they just fade a streak nobody reads as a
sun going behind a mountain.

The authoring comment says why: a near sun's full burst was tried and rejected
as "unpublishable under the palette law (every glow level x every lambert shade
of the island; 257-309 colours at k 12-30)". So the interesting version was cut
for the 256-colour ceiling and the cheap version shipped.

That makes this a palette problem, not an occlusion problem. Two things have
changed since that decision:

1. `island_flat = true` is already set for this subject, so the "every lambert
   shade of the island" half of the product is largely gone.
2. The trail work on 2026-08-18 showed the reconstruction trick that keeps cost
   bounded: composite into ONE six-bit intensity plane and do a single class
   ramp lookup at the end. However many falloffs overlap, the result is at most
   64 colours. The same shape applies to a glow chain over a flat island.

So the fix is to give this subject a sun with a real disc and halo, let it pass
behind the island silhouette, and keep the colour cost bounded by compositing
the glow through a single ramped plane rather than summing graded levels
directly into RGB. Then the existing probe and 15-frame fade have something
worth fading. Verify by DECODING frames and looking at them, not by CRC.

### RESOLVED: big suns smear forward and sideways, should trail only

Owner, 2026-08-18: "The big suns' smear now extends to the top and bottom and
front of the star, should only be at back."

This is a REGRESSION I introduced in `5755d5c` today, and it is live on the
site. Diagnosis, including what I got wrong:

Each history entry stamps a FULL DISC of radius `ghost_r_px` centred on a past
position. The class portraits drift about 9.7 px per frame. So the newest ghost
sits only ~9.7 px behind the sun. A disc of radius R centred 9.7 px behind the
current position extends `R - 9.7` px IN FRONT of it, and R px above and below.
With R now at 40-42 px for the giants, that is ~32 px of smear ahead of the
star and a full 42 px halo of it above and below. Exactly what the owner sees.

The radius I replaced was 9-15 px, and the comment said the ghost radius "sits
at/below the per-frame spacing". I read that as a palette argument and checked
only the palette, which held. It was ALSO a geometry constraint: R <= spacing
is precisely the condition for a round stamp not to bleed past the sun it
trails. I removed the constraint without noticing it was load-bearing twice.

The owner's actual requirement is unchanged and still right: a big sun must
leave a big smear. The smear's WIDTH should be the sun's width. Its EXTENT must
be backward only. A disc stamp cannot express that, which is the real defect.

Fix options, cheapest first:
1. Clip each ghost stamp to the half-plane behind the current position along
   the travel direction (`dot(pixel - now, velocity) <= 0`). Keeps sun-sized
   width, removes all forward bleed. The sideways extent stays R, which is
   correct: that IS the sun's width.
2. If top/bottom still reads as too much, make the stamp anisotropic: full
   radius along travel, reduced across it.
3. Do NOT simply revert to 9-15 px. That restores the uniform-dot defect the
   owner reported earlier the same day.

Verify by DECODING frames and looking at them. The CRC cannot see this, and I
shipped it because I checked the CRC and the palette count and looked only at
noctis-flare, whose sun is small enough that R <= spacing still held.

### Open: the language is still called Form in the site and the repo

Owner, 2026-08-18: "website still says Form language instead of Nanquan. Our
repo also still might contain remnants. Update docs."

Surveyed. There are remnants, and one of them is not just a name.

Site (`zhaozhou-site/template/index.html`, the copy source of truth):
- phase 1 row: "Specification, Form IR and oracle skeleton"
- phase 3 row: "Software console and minimal Form language"
Edit the TEMPLATE, never `public/index.html`, then re-assemble and deploy
through `deploy.ps1` so both copy gates run.

Repo, roughly 16 files outside `compiler/`, notably:
- `AGENT_START_HERE.md` (several)
- `design/blocks.yml` block `SW.COMPILER.FORM`, name "Form compiler"
- `design/contracts/SW.COMPILER.FORM.md` (contract title, `.form` source
  extension, `FORM-E-nnn` diagnostics, `form:gen`/`form:check`)
- `design/contracts/SW.TOOLS.ASSET.md`
- `docs/naming.md` which still states the language "lives in this repository
  under `compiler/`" - that is now false and is the sentence most likely to
  mislead the next agent
- `FORM_LANGUAGE_HARDWARE_CODESIGN.md` (whole document, and its filename)

The part that is NOT a rename: `compiler/` (2.6 MB) is still in the Zhaozhou
tree and the hardware C++ tests still BUILD against it:

    tests/CMakeLists.txt:216,651,729  -> ${CMAKE_SOURCE_DIR}/compiler/tests/generated
    tests/unit/test_tables_tri.cpp    -> compiler/src/generated/tables.ts

So `compiler/` cannot simply be deleted; `crater_ring.hpp` and the generated
`tables.ts` are real inputs to the hardware test lane. Untangling that is the
actual work: either move the generated artifacts the hardware needs into a
hardware-owned path, or vendor them, so Zhaozhou stops depending on a language
tree that now lives in the Nanquan repo.

Suggested order:
1. Site copy (small, visible, deployable on its own).
2. `docs/naming.md` and `AGENT_START_HERE.md`, since those actively misdirect.
3. Ledger/contract rename `SW.COMPILER.FORM` -> Nanquan. Note `ledger:check`
   validates block ids and contract paths, so rename both together and re-run.
4. The `compiler/` build coupling, last and on its own commit.

CORRECTION (owner, 2026-08-18): the language was ALWAYS called Nanquan. "Form"
was never a real name; it entered the tree through a miscommunication with an
agent that did not know the name. There is no history to preserve here.

So this is fixing an error, not renaming a thing. Practical consequences:

- Replace "Form" with "Nanquan" everywhere it names the language, including in
  old documents and provenance headers. Do NOT add notes saying it "was
  formerly called Form" and do NOT keep the old name for continuity. That would
  preserve a mistake as though it were a decision.
- `FORM_LANGUAGE_HARDWARE_CODESIGN.md` should be renamed to
  `NANQUAN_LANGUAGE_HARDWARE_CODESIGN.md` and its contents corrected outright.
- `SW.COMPILER.FORM` in the ledger, `design/contracts/SW.COMPILER.FORM.md`, the
  `FORM-E-nnn` diagnostic prefix and the `.form` source extension are all the
  same error and should all become Nanquan forms. Check whether the Nanquan
  repo already fixed the diagnostic prefix and file extension, and match it
  rather than inventing a second convention.
- Keep the ordinary English word "form" where it genuinely means shape or a
  form of something. A blind replace will corrupt those; several exist in the
  co-design document already.

#### Same item: the site's dashboard and progress claims are now stale

Owner added: "also update dashboard and progress stuff on website."

Today's work falsified several statements on the page. They must be corrected,
and corrected WITHOUT overclaiming - the page's whole value is that it does not
soften hardware claims. Concretely, in `zhaozhou-site/template/index.html`:

Phase table:
- Row 1 and row 3 still say "Form" (see the rename item above).
- Row 4 "First exact tile and triangle: the hardware rasterizer begins" is
  marked `not started`. That is now false. `RASTER.EDGEWALK` is implemented in
  RTL, differentially tested against the C++ oracle, formally proven for the
  top-left rule, and mutation-checked. `RASTER.TILESTORE` and `RASTER.RESOLVE`
  are in flight.

"What simply is not built yet" section:
- "The hardware rasterizer is not started. Its five blocks are
  specification-only and its RTL directory is empty." Every clause of that is
  now wrong. `fpga/rtl/raster/` contains `zhao_raster_edgewalk.sv` and
  `zhao_raster_fill.sv`.
- The neighbouring claim "Every image on this page came from the software
  renderer" is STILL TRUE and must stay. The rasterizer is simulated and
  formally proven; it has produced no published image.

What the replacement wording must preserve:
- simulated and formally proven is NOT synthesized, and neither is on-hardware
- no board has been probed; phase 0 stays BLOCKED
- "Per-block resource figures and timing closure" stays unclaimed

New, honest, and worth stating because it is a measured result rather than a
guess: full-shell synthesis was ATTEMPTED on Quartus 17.0.2 Lite and does not
fit this machine. `quartus_map` committed 28.4 GB against 24 GB of RAM and
thrashed at near-zero CPU. A single leaf module elaborates cleanly in 67 s with
a 4.8 GB peak just to parse the 22-file cone. So the toolchain works, the design
is not at fault, and per-subsystem characterization is the lane that fits. That
is a better sentence than "the synthesis toolchain is open locally" because it
reports an experiment instead of a capability.

Edit the TEMPLATE only. `public/index.html` is generated and overwritten.
Deploy with `deploy.ps1` so both copycheck gates run. Watch the copy rules: no
em dashes, no en dashes used as dashes, no banned phrases, no defensive
frame-rate prose.

## Wave 4 progress: three phase-4 blocks in RTL

`RASTER.EDGEWALK` (`8ad678a`, `d20d56f`, `d19ee93`), then `RASTER.TILESTORE`
and `RASTER.RESOLVE` (`da4960c`, `1a77214`, `c25da82`, `7d68c2f`, `aa2120b`).
Both increments were built by one agent at a time and reviewed here before
pushing. Verified independently rather than on the agents' reports:

- Full fast lane on the final tree: **96/96, 0 failures**. Not argued by
  composition; actually run.
- Format gate: 123 files clean under the pinned LLVM 15.
- EDGEWALK fill rule: algebra checked by hand against `rast.cpp`
  (`E0 = 256*E' + r`; `r` is constant per tile because a pixel step is 256
  subpixels, so both edge steps are multiples of 256).
- EDGEWALK mutation-checked HERE, not just by the agent: injecting the
  top-left defect took the directed suite to 11/148 failed with the right
  symptom (oracle covers 16 diagonal pixels, mutant covers 0), and the formal
  proof failed too. The suite can go red, so its green means something.

Maturity stays SPECIFIED for all three. Simulated and formally proven is not
synthesized and is not hardware.

### RESOLVED (owner, 2026-08-18): black does not resolve to black

Verified independently by evaluating the oracle's own expression over all 16
Bayer cells. Green's dither amplitude is 32 while red and blue get 16, so for a
black pixel the green numerator is `32*B + 16`, which reaches 255 at `B >= 8`:

    B = 0..7   -> 0x0000   correct black
    B = 8..15  -> 0x0020   green level 1

Half of every black region resolves to a faint green speckle. This is the
counterpart of the white-rail defect fixed 2026-08-16, and it is the same class
of bug left half-done. It matters here specifically because the game is set
against night skies and space.

The agent handled it correctly: it reproduced `resolve.cpp` bit-for-bit rather
than making the RTL "better" than its own oracle, pinned the real behaviour in
`test_rails`, and escalated. An RTL block that silently disagrees with its
reference is worse than one that faithfully reproduces a known defect.

Owner decision required, because fixing `resolve.cpp` moves every golden
capture's canvas CRC:
1. Fix the law and re-pin all goldens (recommended)
2. Leave it and document the speckle as intended dither behaviour
3. Clamp the black rail only, leaving the rest of the law untouched

### Also found, not acted on (validator-gated, not an RTL whim)

Two stale ledger notes, documented in the contracts instead of edited:
- TILESTORE "M10K budget per memory_rules §7.3" - that file has seven sections
  and no §7.3; the tenancy list is CHARTER §7.3.
- RESOLVE "Dither matrix is a generated table (W3 fixgen), never hand-typed" -
  contradicts `resolve.cpp`, which records that fixgen has no such table.

## Phase 4 core: the three blocks composed

`85e41b1`, `b7eacec`, `95a5395` add `fpga/rtl/raster/zhao_raster_tile_pipe.sv`:
EDGEWALK -> TILESTORE -> RESOLVE wired together, so a triangle now becomes
RGB565 framebuffer words with a deterministic tile CRC. Until this, the three
blocks had never been instantiated together and each contract said so.

Verified here, not taken from the agent's report:
- Full fast lane run on the final tree.
- Ping-pong MEASURED by re-running the directed test myself: 16 tiles
  back-to-back is 5011 cycles (313/tile, max 2 in flight); the same 16 fed
  serially is 8896 cycles (556/tile, max 1). Identical pictures and CRCs both
  ways. That 1.78x is the whole justification for the tile store being
  ping-pong, and it is now a number instead of an argument.

The agent found and fixed a real hole in its own work: the framebuffer-address
mutation was caught by the directed lane only, because the random lane had no
address check at all. It moved the law into a shared helper, wired it into both
lanes, and re-ran. A green random lane there had been meaningless.

It also hit the stale-rebuild hazard and did not paper over it: a batched
mutation sweep produced IDENTICAL failure lists for three different mutations
because ninja skipped re-verilation after same-tick edits, so mutated RTL ran
against a stale model. It redid the entire sweep one mutation per invocation,
asserting the relink each time. Worth knowing for anyone scripting mutation
runs on this toolchain.

Deliberately not done, and I agree with each: no ledger entry invented for the
composition (the RASTER group has five blocks and none is "the composition";
registering one is a validator-gated edit), and no formal property, because the
composition's real laws are dataflow over a 256x64 store and a 259-cycle
stream, far beyond shallow BMC, while the shallow ones would restate the
`assign` they claim to prove. The two arithmetic cores worth proving already
have proofs on the exact modules this block instantiates.

## Synthesis: per-block characterization is producing numbers

`96b627d` adds `tools/quartus/run_block_fit.ps1`. First placed-and-routed
results against the provisional 5CSEBA6U23I7:

    zhao_video_mode    70 ALMs,  58 registers, 0 M10K
    zhao_audio_fifo   222 ALMs, 292 registers, 7 M10K
    zhao_debug_crc    158 ALMs

Two traps cost real time and are commented in the script so they cannot cost it
twice:
- `Start-Process -PassThru` with `-NoNewWindow` and redirected streams returns
  a Process whose `ExitCode` reads back EMPTY. Every stage looked like a
  failure while Quartus was reporting "0 errors, implemented 127 logic cells".
  Caught only by reading the tool's own log instead of believing the script.
  Direct invocation plus `$LASTEXITCODE` is authoritative.
- StrictMode faults on the resource keys a failed fit never gains.

Still not hardware. Provisional device, virtual I/O, no board, and a per-block
fit says nothing about the composed machine's routing or timing closure.

#### Resolution of the smear regression (`c35ab7f`)

The half-plane clip was tried first and rejected on sight: it removed the
forward bleed but left a straight guillotine edge through the smear where the
clip plane crossed the disc. Looking at that instead of shipping it exposed the
real cause, which was mine and one level up.

`ghost_r_px` is the ghost's HALO radius, not "how big the smear should be".
`trail_disc_radius` derives the ghost's disc from it:

    ghost_disc = disc_r * ghost_r / halo_r

Feeding it `disc_r_px` squares the ratio. The orange giant got a 63 px ghost
disc against a real 42 px one. A ghost larger than its own sun spills past it
on every side by construction, which is exactly what shipped.

`ghost_r_px = halo_r_px` makes the derived disc come back as `disc_r_px` on the
nose, so a ghost IS the sun's silhouette: same size, one step behind, nothing
in front. The owner's requirement is still met and met better, because the
smear inherits each sun's actual size (12 px blue dwarf, 28 px orange giant)
rather than the uniform 9..15 band that caused the original complaint.

The confirmation worth trusting: `noctis-flare` is byte-identical before and
after. Its ghost was already `halo_r_px`, and it was the one subject that
looked right.

Verified by decoding frames and looking at them. `reel --check` green on all 18
sequences, all 18 GIFs regenerated from one build and byte-exact verified,
deployed through `deploy.ps1` with both copy gates.

Lesson recorded because it caused this: the earlier comment said the ghost
radius "sits at/below the per-frame spacing". I read that as a palette argument,
checked the palette, found it held, and changed the value. It was load-bearing
for geometry too. A comment that gives one reason is not proof there is only
one.

## RASTER subsystem complete: all five ledger blocks in RTL

`34d8373`..`d4b995b` add `RASTER.EARLYZ` and `RASTER.FRAGMENT`, and replace the
tile pipe's stand-in write path with the real chain. The ledger's RASTER group
is now five blocks and all five exist:

    TILESTORE  EDGEWALK  EARLYZ  FRAGMENT  RESOLVE

The composed pipe is now EDGEWALK -> EARLYZ -> FRAGMENT -> TILESTORE -> RESOLVE.
Throughput re-measured at 316 cycles/tile back-to-back against 313 before: a
1.0% regression that is exactly the new chain's fill-and-drain (1 cycle EARLYZ,
2 FRAGMENT) paid once per tile at the `pipe_empty` swap gate. Steady state is
unchanged at one covered pixel per clock.

### A coordination hazard I caused

The agent reported `reel_sequence_crc` RED at its baseline `625d327`, and it was
right. I was editing `tools/reel/zhao_reel.cpp` for the smear fix in the SAME
working tree while it ran its suite, so its baseline was poisoned by my
in-flight edit until `c35ab7f` re-pinned the CRCs. It correctly attributed the
recovery to my commit instead of claiming it. One shared tree plus a running
agent means edits to shared files are visible to its test runs; either stay out
of the tree while an agent runs, or expect to explain the noise.

### What the increment got right and is worth keeping as the standard

- Found FOUR holes in its own composed lane and fixed them rather than noting
  them: the pipe drove `state == 0` for every job so there was nothing to
  invert, the recipe vector never railed, depths were uniform over 24 bits so
  the one-LSB early-Z boundary was a 1-in-16M shot, and the composed picture
  cannot observe early-Z PESSIMISM at all, so both lanes now diff
  `early_z_rejects` / `blended_fragments` against the oracle directly.
- Declined three things for stated reasons: no extra depth comparison modes
  (spec/qformats.md §8 defines exactly one, so an enum would be law invented in
  RTL), no stencil-only ops (TILESTORE has no byte enables), and no formal
  property on EARLYZ, because its interesting invariant is over state it cannot
  see and proving it would prove a model rather than the shipping bytes.
- Pinned the ABSENCE of per-pixel fog. `spec/qformats.md` §8 freezes fog as
  per-vertex in GEOM.PROJECT and records that a per-pixel form is "not costed,
  not built". The ledger's purpose line predates that. Rather than build to the
  stale line or silently skip it, the directed test computes the frozen vertex
  mix and requires it to reach the tile unaltered, so a block that grew a fog
  stage would double-apply and go red.

Verified here: the tag-never-dithered law holds in `zhao_raster_resolve.sv`
(`px_tag = tr_data_i[39:32]` straight through, no quantizer, not in the CRC),
so there was genuinely nothing to patch. Format gate clean on 136 files.

Still SPECIFIED for both blocks. Simulated and formally proven is not
synthesized, and neither is on-hardware.

## Synthesis sweep progress

Per-block fits landing steadily against the provisional 5CSEBA6U23I7:

    zhao_video_mode      70 ALMs      zhao_scanout_fetch   208 ALMs
    zhao_audio_fifo     222 ALMs      zhao_video_framectl  301 ALMs
    zhao_debug_crc      158 ALMs      zhao_video_scaler     52 ALMs

`zhao_video_scanout` exceeded its 900 s budget and is recorded as `timeout`
rather than given a fabricated number. Note a flaw in `run_block_fit.ps1`: the
timeout is checked BETWEEN stages, so it detects an overrun after the fact
instead of preempting it. The status means "exceeded budget", not "was killed".

## Phase 5 complete: textures and the geometry front

Two increments, one agent at a time, both reviewed here before pushing.

### TEXTURE.CACHE and TEXTURE.TMU

`zhao_texture_cache.sv` (four independent direct-mapped lanes, so a bilinear
sample is ONE cache access), `zhao_texture_tmu.sv` + `zhao_texture_bilerp.sv`.
Nearest is bilinear with both fractions forced to zero, so charter §26's "no
second unrestricted sampler" is satisfied structurally rather than by promise.
Integrated into RASTER.FRAGMENT's texel ports in both directions WITHOUT
changing that interface, which is what it was designed for.

Verified here: fast lane 111/111.

The increment set a standard worth keeping: it listed five laws it CHOSE rather
than found (bilinear weights and their rounding, half-texel bias, mip selection,
row-major layout, colour expansions) and recorded each in both the RTL header
and the contract. Charter §15 asks for Morton swizzle, but no Morton formula is
ratified anywhere, so it used row-major and said so instead of inventing one and
presenting it as law.

It also reported a ledger target it did NOT meet, with the measurement: "1
sample per clock" is actually 1 per 4 clocks direct, 1 per 6 CLUT.

### GEOM.CLIP, GEOM.SETUP, GEOM.BINNER

`zhao_geom_clip.sv`, `zhao_geom_setup.sv`, `zhao_geom_binner.sv` +
`zhao_geom_arena.sv`, plus `zhao_geom_bin_pipe.sv` which drives the REAL
rasterizer from real tile lists.

Laws found rather than invented, each cited: the near plane is a whole-primitive
rejection and not a clip (so CLIP needs no divider at all), the guard band is an
upstream saturation in `to_screen_xy` rather than a clip plane, the scissor test
is `raster_tri`'s own early return (extracted as `zref::render::scan_bbox` so
the law has ONE site and the oracle calls it), and the edge function's third
constant is free by the barycentric identity, which saves exactly 2 of 6
multipliers.

Three findings worth keeping:

1. Wiring BINNER to the real rasterizer found a real bug immediately: the drain
   port emitted the tile INDEX where EDGEWALK wants the tile's top-left PIXEL, a
   silent 16x error no tile-indexed differential could ever see. That is the
   argument for composing blocks rather than testing them alone, demonstrated
   instead of asserted.
2. One mutation was caught by NEITHER lane and the increment fixed the tests
   instead of arguing. The §8 flooring-versus-truncation difference only shows
   for negative non-multiples of 256, and 20,000 random triangles gave zero hits
   (measured). So it constructed a witness: for triangle (0,0),(W,0),(0,W) the
   value at pixel (0,0) is exactly W*(W-256), so W=255 gives -255. Directed case
   plus controls at 254 and 256, plus a random lane that asserts it reaches the
   window.
3. A Verilog signedness trap cost a real bug: a comparison goes unsigned if
   EITHER operand is, so a negative `kx` tested as positive and 29 tiles
   vanished. Recorded in `GEOM.CLIP.md`'s Q-formats section so the next reader
   does not repeat it.

Ledger targets not met, measured and recorded as shortfalls rather than left
unexamined: BINNER is 2.83 cycles per bin reference against a target of 1, and
GEOM.SETUP's gradient half is not built because `rast.cpp` re-derives each row
start with an independent `div_rhu_s128` (so a plane-setup block would not be
bit-exact against the oracle) and RASTER.FRAGMENT interpolates nothing yet.

Verified here: fast lane 122/122, exit 0. Format gate clean on 154 files.

All blocks stay SPECIFIED. Simulated and formally proven is not synthesized, and
neither is on-hardware.

## Docket: the eclipse battle scene (owner, 2026-08-18)

Owner, flagged by them as a late request with the hardware already well along:

> I want a scene in the game to be players with their creatures on the
> battlefield. It's a beautiful day, there are birds and chirp sounds. The sun
> is awesome. Then the moon slowly moves in front of the sun, maybe because of
> a spell. The entire battlefield goes dark. There is thunder, lightning, it
> rains from the clouds above, the moon is in front of the sun, slowly there is
> an eclipse, and there is an amazing corona for the sun.

This is a signature set piece, not an effect. Beat sheet as described:

1. Bright day. Players and their creatures on the battlefield. Birds, chirping.
   The sun is the hero element.
2. A moon enters, slowly, possibly spell-driven.
3. The battlefield darkens as occlusion grows.
4. Weather turns: thunder, lightning, rain from the clouds above.
5. Totality, with a corona that is the payoff shot.

### Why it is a good late request rather than a bad one

Almost every system it needs is already specified or built, and it exercises
them together, which is exactly what a set piece should do:

| beat | what it leans on | state |
|---|---|---|
| sun, corona | `stars_and_flares.md`, `star_compose.cpp`, the flare chain | built (reference) |
| occlusion and the fade | the occlusion probe + 15-frame fade | built, but see the flare-occlusion docket item, it currently has nothing worth fading |
| darkening | environment / `SetEnvironment` (ABI v3, landed today) | ABI exists |
| clouds, rain, lightning | `sky_and_beams.md` cloud sheet; particles are phase 10 | sky partly, particles not started |
| creatures on the field | creature system, phase 9 | reference only |
| birds, chirps, thunder | audio; `zhao_audio_fifo` exists in RTL and is fitted | transport exists, content does not |

So the honest read: the scene is composable from the reference renderer FIRST,
as a reel subject, well before the hardware can carry it. That is also the
right order, because it would pin down what the eclipse actually needs before
any of it is committed to RTL.

### The one hard part, named early

A real eclipse is not the existing occlusion path. The current probe is a
binary "is the disc hidden" gate driving a fade. An eclipse needs PARTIAL
coverage that grows: a moving occluder disc subtracting a lune from the sun's
disc, with the corona surviving after the photosphere is gone. That is a
different law, and it is the thing that makes the payoff shot work. It also
interacts with the palette ceiling, because a corona against a darkened sky is
exactly the wide-gamut case the 256-colour budget has already forced
compromises on twice today (see the flare-occlusion diagnosis above).

### Suggested order when this comes up the queue

1. Amend the occlusion law from binary-gate to fractional coverage, in the
   reference, with the corona surviving totality.
2. Build it as a reel subject: sun, moon crossing, darkening, corona. Judge it
   by decoding frames and looking at them.
3. Add weather (cloud darkening, rain, lightning flashes) once the eclipse
   itself reads correctly. Lightning is a full-frame additive flash, which is
   cheap and dramatic and should be authored against the palette counter.
4. Creatures and audio last: both are real subsystems with their own phases,
   and neither is needed to prove the scene's look.

Do NOT start this before the flare-occlusion item, since that item is the same
code path and fixing it is a prerequisite rather than a parallel task.

## Docket: the set-piece catalogue (owner, 2026-08-18)

Extending the eclipse request into a family. Owner's words:

> similar scenes for bloodmoons, sunsets, sunrises, blood rainbows where a
> rainbow persists into an extremely blood red scene. Find a few more of these
> amazing lookers of scenes. Maybe a giant asteroid falls, the clouds blow
> apart. A huge volcano in the background (probably just part of the skybox, not
> simulated) spews ash into the air and it covers everything. Lots of events
> like this.

### The owner's own insight, which should become a rule

"probably just part of the skybox, not simulated" is the right instinct and
deserves to be a general principle, not a one-off concession. A background
volcano that is a drum-band element costs a texture and a scroll; a simulated
one costs terrain, particles and bandwidth. The catalogue below is therefore
split by COST CLASS, because that is what decides whether a scene is a week or
an afternoon:

- **Class S (skybox):** lives in the sky drum. Bands, cap, scroll, additive
  layers. Cheap, and `sky_and_beams.md` already carries the machinery.
- **Class P (palette / environment):** a global colour law. `SetEnvironment`
  landed in ABI v3 today, so a scene that is "the same world, different light"
  is nearly free.
- **Class E (event):** needs a moving occluder, a particle burst, or terrain
  response. Real work, real budget.

### The catalogue

| scene | class | what carries it | what it needs that does not exist |
|---|---|---|---|
| Eclipse (see the item above) | E | sun + moon occluder + corona | fractional occlusion law |
| Bloodmoon | P + S | environment palette shift, red moon in the drum | a moon sprite in the sky set |
| Sunset / sunrise | P + S | band gradient + sun altitude + warm horizon | nothing; `sky-dusk` already proves the bands |
| Blood rainbow | S | an additive arc layer over a red-shifted sky | an arc primitive; the palette fight is the real cost |
| Asteroid fall, clouds blown apart | E | a streak, then a cloud-sheet displacement | cloud sheet needs a radial displace; particles are phase 10 |
| Background volcano spewing ash | S then P | drum band with an animated plume, then a global ash tint | an animated drum layer; the tint is `SetEnvironment` |
| Ash covering everything | P | environment tint plus a surface-sheet darkening | surface sheet is phase 6, in flight |

### A few more, since the owner asked for suggestions

- **Aurora over a night battlefield.** Class S. Vertical additive curtains in the
  drum with a slow shear. Reads as expensive, costs almost nothing, and the
  six-bit additive plane already exists for exactly this kind of glow.
- **Twin suns setting out of phase.** Class S + P. The star gamut already has
  twelve classes and `multiple` proves two bodies. Two different-class suns at
  different altitudes give two shadow colours and one very strange sky.
- **Moonrise behind a storm, lightning silhouetting the clouds.** Class S + E.
  Lightning is a full-frame additive flash, the cheapest drama available, and it
  makes a static cloud sheet look alive.
- **Total darkness with only spell light.** Class P. Set the environment near
  black and let additive effects be the only illumination. Cheapest scene in the
  list and probably the most atmospheric.
- **Falling ash after the volcano, settling on the terrain.** Class P + E. The
  interesting half is persistence: the surface sheet already holds scars, so ash
  that ACCUMULATES rather than merely tinting would reuse a mechanism that
  exists.
- **Sun through a dust storm, the disc reddened and the corona gone.** Class P.
  A single law: attenuate the flare chain and shift the ramp. Pairs with the ash
  scene and proves the environment lane end to end.

### The constraint every one of these hits

The 256-colour-per-sequence ceiling. It has already forced compromises twice
today: the flare-occlusion subject was cut to a stripe-and-dot for it, and
noctis-flare sits at 248 of 256. Every scene above is a wide-gamut scene by
nature, so the palette is the design constraint, not the effect list.

The trail work today found the technique that survives it: composite the glow
into ONE six-bit intensity plane and take a single ramp lookup at the end, so
however many layers overlap, the result cannot exceed 64 colours. Any of these
scenes that stacks glow should be built that way from the start rather than
summing graded levels straight into RGB and then discovering the ceiling.

### Suggested order

Cheapest-first, because each one that lands makes the next easier to judge:
sunset/sunrise, then total darkness, then bloodmoon, then aurora, then the
volcano plume, then ash accumulation, then the asteroid, and the eclipse
whenever the fractional occlusion law is done. Blood rainbow last: the arc
primitive is new and the palette fight is worst there.

## Docket: stronger Noctis flares as a reusable feature (owner, 2026-08-18)

> Make the noctis flares stronger if anything. We can put them on various things
> and I want it to be a thing.

Agreed, and there is a specific blocker to clear first, because "stronger" and
"on various things" both cost the same scarce resource.

`noctis-flare` currently sits at **253 of 256 colours** with the v1.3 trail. It
is the busiest subject in the gallery: flare chain plus retained-history trail.
So the flare cannot get stronger, and certainly cannot be applied to other
subjects on top of their own content, until its palette cost comes down.

### The enabling change, and it is already proven elsewhere

The trail work today established the technique: composite the glow into ONE
six-bit intensity plane and take a SINGLE class-ramp lookup at the end. However
many falloffs overlap, the result cannot exceed 64 colours, because 64 is all
the ramp has.

The flare chain does not do this. It sums graded levels straight into RGB, so
every additional flare element multiplies against every background colour it
lands on. That is why it is expensive and why it had to be cut to a
stripe-and-dot in `flare-occlusion`.

Route the flare chain through the same single-plane reconstruction and its
palette cost collapses to roughly the ramp size. That is what buys BOTH a
stronger flare AND the ability to put flares on various things, which is what
the owner actually asked for. Do this before tuning any flare strength upward,
otherwise every increase gets refused by the palette counter and the real
answer looks like a limitation.

### Then, the strength itself

Once the cost is bounded: raise the burst intensity, extend the ghost chain,
widen the anamorphic streak, and let the glow tag drive a brighter bloom.
Author each against the palette counter, which stays the arbiter.

## Docket: lightning, fire, and water (owner, 2026-08-18)

> a pass for really good lightning and fiery fire effects should also be on the
> docket. And a bit for water. We already have the terrain looking like water in
> deformation, might as well use that for actual water wave physics. Maybe a
> little wizard dude can run across so fast he doesn't sink.

### Lightning

Cheap and dramatic, and it should be built early for that reason. A full-frame
additive flash costs almost nothing and makes a static cloud sheet look alive.
The bolt itself is a jagged additive polyline with a bright core and a wide dim
halo; the flash is the payoff, not the geometry. Two laws worth pinning up
front: the flash must be a single environment-level term so it cannot multiply
the palette, and the bolt's branch recursion must be seeded deterministically so
replay stays byte-exact.

### Fire

Harder than lightning and worth doing properly. Fire is the case where a
palette-indexed rotation earns its keep: a CLUT cycling through a heat ramp gives
motion for free, which is exactly the trick `star-boil` already proves at 191
colours. Build it as a ramp rotation over a noise field rather than as particles,
at least first; particles are phase 10 and fire does not need to wait for them.

### Water, and the owner's own good idea

> We already have the terrain looking like water in deformation, might as well
> use that for actual water wave physics.

This is the strongest suggestion in the set, because the machinery exists. The
`wave_pool` field program already produces travelling sinusoidal displacement
over the lattice, and `terrain-wave` ships it. What separates that from water is
not the waves, it is:

1. a material and shading treatment (specular, transparency, a horizon tint),
2. sustained motion rather than a bounded event, and
3. a response to things moving through it.

Point 3 is where the wizard comes in. "Runs across so fast he doesn't sink" is a
lovely mechanic and it is also a physics statement: contact time versus
displacement response. If the surface's restoring force is slower than the
runner's footfall interval, the runner stays on top. That is a genuinely cheap
rule to implement on a lattice that already has velocity, and it makes the
mechanic emerge from the simulation rather than being scripted, which is much
better. Worth prototyping in the reference renderer as a reel subject: a fast
mover leaving a wake and staying up, then slowing and breaking through.

### Order

Lightning first (cheapest, biggest immediate payoff, and the set-piece catalogue
above wants it). Then water as a reel subject to prove the wake mechanic. Then
fire via CLUT rotation. All three before any of it goes near RTL, since the
reference renderer is where the look gets settled and none of these are
hardware-blocked.

## The black rail, resolved at the cause (owner decision)

Owner: "you asked way back if it was ok for the black triangle to have green
stuff. That's not okay, we do it right from the start."

Investigating to fix it found something better than a patch. `resolve.cpp`'s own
header states the law as "the dither threshold t = (B+0.5)/16 of one
quantization step". One quantization step is 255 numerator units for ALL THREE
channels, because all three divide by 255. So that threshold is `(B*16 + 8)`
everywhere. Green was implemented as `(B*32 + 16)`, DOUBLE its own stated law.

The doubled amplitude broke both rails:

    black,  g=0   : (32B + 16)/255 = 1 for B >= 8, so half of every pure-black
                    tile resolved to 0x0020. Red and blue, at the correct
                    amplitude, top out at 248/255 and stay exactly 0.
    white,  g=255 : (255*63 + 496)/255 = 64, which wrapped the six-bit field.
                    That was patched on 2026-08-16 with a clamp. The clamp
                    treated the symptom; the cause was always this amplitude.

At the documented `(B*16 + 8)`, verified across every Bayer phase: black gives
{0}, white gives {63}, and mid-tones still dither ({31,32} at g=128). The
`min()` clamps stay as belt and braces rather than as load-bearing repairs.

So the white-rail fix in August was half a fix. Correcting the amplitude closes
both, and it makes the code agree with the sentence directly above it, which it
had contradicted since it was written.

Blast radius: every golden capture CRC that contains a resolved frame moves.
That was the reason this was escalated rather than fixed unilaterally, and the
owner has now called it.

## Sun trails v1.3 series, and what the dither fix cost

Owner reported four things across the session. All four are fixed, all verified
by decoding frames and looking at them rather than by CRC.

| report | cause | fix |
|---|---|---|
| trails at the TOP of horizontally moving suns | Noctis's kernel samples down-right, so energy always moves up-left regardless of heading. The documented v1.2 law. | v1.3: offsets rotate onto the direction of travel |
| orbiting pair's trails JUMP between orientations | v1.3 chose one of EIGHT lattice directions, which snaps as a binary's heading sweeps | v1.3a: taps are a Bresenham line along the true heading, changing one texel at a time |
| not smeary or hazy enough | four narrow taps cannot dissolve ghosts ~9.7 px apart | eight taps reaching four texels along the motion plus across-axis spread |
| not strong enough | decay 8 from a six-bit value extinguishes a trail in 8 ages, so the ring's oldest entry contributed exactly nothing | decay 4, so all eight retained ages carry energy |
| blue star has no animated sheen | SATUR = min(63, 12d/r) is a FLOOR that boil_index clamps every entry up to; at 2.5r it was 30 and the bottom half of the ramp went flat | 1.5r, so the floor is 18 like star-boil. Closer also REDUCES white wash, so class colour is better preserved |
| trails should be asymmetric, throwing sunstuff | stamps were perfectly circular | v1.3b: one prominence lobe, rotating with the star, seeded per body so a binary's two suns do not eject alike |
| parts should be lesser, ephemeral, slightly off | a perfectly graded smear reads as computed, not retained | v1.3c: deterministic per-age intensity variation and +-1 texel jitter |

Every one of these is deterministic, so replay stays byte-exact and the capture
law is untouched.

### What the dither correction cost, measured

Correcting green's amplitude UN-COLLAPSED greens that the doubled value had been
merging, so colour counts moved. `noctis-flare` went to 284 against a ceiling of
256. Measured rather than guessed:

    flare chain alone (ghost_r_px = 0) : 245 colours
    trail on top                       : ~39 more
    shrinking the trail to 10 px       : still 270

So the trail is not the cost, the chain is. Moving the sun from k=40 to k=20
fits the subject at 240 AND makes the burst bigger, which is the direction the
owner asked for, so the constraint and the request happened to agree.

This is the docketed blocker with a number on it: the flare chain sums graded
levels straight into RGB, so its cost is (backgrounds x levels). Route it
through ONE six-bit plane with a single ramp lookup at the end, the way the
trail already is, and the cost drops to about the ramp size. Until that lands,
`noctis-flare` is the ceiling case and every change to it must be measured.

### Test changes this forced, and why none of them is a weakening

- Six separate files carried their own copy of the dither law with green's wrong
  amplitude. All six corrected: `render_sky` (x3), `render_directed`,
  `render_heightfield`, `terrain_dual`, `texture_mosaic_directed`.
- THREE tests had pinned the defect as expected behaviour, e.g. "black rail:
  green is lifted to 1 at exactly the 8 Bayer values >= 8 (128 px)". That was
  the honest thing to do when the behaviour was observed-not-endorsed. They now
  assert black is black.
- `render_star`'s impulse anchor could no longer be a single hand-computed
  number, because v1.3c deliberately varies each age. The DIRECTION law is still
  asserted exactly (nothing at or ahead of the source, which is what v1.2 got
  wrong); the magnitude is asserted over the region the jitter can reach. The
  source was widened from one texel to 5x5 because a single texel can now be
  attenuated to nothing, which would have made the direction checks VACUOUS
  rather than failing.
- `render_golden`'s two CRCs re-pinned with the reason recorded inline, matching
  the file's existing convention of logging every re-pin and its cause.

### A trap that cost real time, third occurrence today

`ninja: no work to do` after editing a `.cpp`. I iterated several times on
`render_star` against a STALE binary before noticing the build was a no-op.
Earlier the same day it was the verilate copy-scripts (deleting them makes the
build fail silently and the old exe run) and mtime-touching (poisons ninja's
next comparison). The rule that actually works: check that the thing rebuilt,
by hash or by the linker line, before believing any result.

## Phase 6 complete enough to render: terrain reaches the framebuffer

`TERRAIN.PROJECT` and `TERRAIN.LOD` landed, and with them the chain closes.
`tests/terrain/terrain_project_chain.cpp` wires FOUR Verilated models with no
adapter:

    TERRAIN.PROJECT -> GEOM.CLIP -> GEOM.SETUP -> zhao_geom_bin_pipe
                                                 (BINNER -> EDGEWALK -> EARLYZ
                                                  -> FRAGMENT -> TILESTORE
                                                  -> RESOLVE)

Real terrain vertices in, RGB565 framebuffer words out, checked against the
oracle at every stage. The case worth keeping: a near camera with the eye INSIDE
the patch drops 8 triangles for a behind-the-eye corner and still draws 24, 174
records, 15,489 px. That is `terrain.cpp`'s "a near camera used to erase the
island" law holding through actual hardware rather than through the software
renderer.

`terrain_lod_tess.cpp` drives the real tessellator from the real LOD block:
across all 24 interior subpatch boundaries the two sides emit the identical
vertex set on the shared edge, and the patch tiles exactly (signed areas sum to
-2A).

Verified here: 140/140, exit 0, format gate clean.

### What composing exposed, again

1. `TERRAIN.TESS` carries one `src_id` per JOB, not per triangle, so a triangle
   cannot be followed from world vertex to the framebuffer word it produced.
   That is a charter `source_ids` gap and it only becomes visible when blocks
   are wired together.
2. There is no vertex-to-triangle assembler in the ledger between projection and
   clipping. The increment made PROJECT triangle-in/triangle-out rather than
   hiding the missing block behind C++ glue, and recorded the cost: a shared
   lattice vertex is projected up to six times. `GEOM.WCACHE` is the fix and it
   is phase 8.

### Three mutations came back green, and they were findings

- A uniform row-sum scaling is invisible to the chain because projection is
  HOMOGENEOUS: ndc is unchanged, only `1/w` moves, and the depth test is off.
- A B/C vertex swap is invisible because `GEOM.CLIP` normalises winding.
- Random input never produces `clip.w == 0` or `dev == distance` exactly, which
  is the entire reason hand-built boundary cases exist.

Meanwhile a transposed neighbour index left BOTH blocks internally consistent
and only the composition called it a tear. That is the argument for composition
tests in one line.

### Three coverage counters were zero while every differential passed

Lane A started every subpatch at the finest level, so the ladder only ever
walked one way; lane B drew the morph factor uniformly, so a refine could never
commit; lane B's coordinates sat at the word limit where every level passes. All
three fixed, and the lanes now assert they reach those states. A green random
lane that sampled nothing is not evidence.

## Synthesis reaches the new RTL

`31db6cf` extends `run_block_fit.ps1` with `-ExtraSources`, because everything
built today lives outside the shell cone and the runner could not reach any of
it. Same QSF, same flow, so one lane characterizes both rather than a second
drifting from the first.

First placed-and-routed results for work from this session:

    zhao_raster_fill    23 ALMs
    zhao_raster_quant   39 ALMs

A sweep of the remaining 19 new modules is running. Limitations unchanged and
carried in the JSON: provisional device, virtual I/O, no board, and a per-block
fit says nothing about the composed machine's routing or timing closure. Phase 0
stays blocked. Nothing has been programmed onto a device.
