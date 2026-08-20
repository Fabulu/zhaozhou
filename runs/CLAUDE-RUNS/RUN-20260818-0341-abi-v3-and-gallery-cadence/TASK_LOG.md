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

## Phase 6 closed to 10 of 11, and Phase 7 opened

`SURFACE.SHEET`, `SURFACE.STAMP` (+ a `zhao_surface_blend` split out so it could
be proved), `TEXTURE.MOSAIC` (+ `zhao_texture_mod255`), `TEXTURE.AUX`,
`FORGE.CLIFF`, and then `TERRAIN.BAKE` (+ `zhao_terrain_bake_delta`) all landed.
`FIELD.SEQ.STAMP` is the only phase-6 RTL block not built, and it was refused
deliberately: a 30-opcode Field IR processor whose own named reference does not
exist and whose upstream `FIELD.PROGCACHE` is phase 7. That was the right call.

Verified here at each step: 148/148, then 154/154, both exit 0, format gate
clean.

### Two refusals worth keeping

`STAMP -> SHEET -> PATCH` was NOT wired, and the reason is correct: `PATCH`'s
`scar_i` is layer B (height16, per-vertex, 33x33) while `SHEET` owns layer F
(`{tag u8, strength u8}`, 64x64), and `terrain_rules §7` says B is written only
by `TERRAIN.BAKE`. Faking that seam would have meant inventing a format
mapping. `TERRAIN.BAKE` is now built, so the chain became wireable and a
`terrain_bake_chain` test exists.

`FORGE.CLIFF`'s wall-vertex emission was left out because it needs the
tessellator's stitched edge set, layer C, and a stateful rim-length accumulator
— none of them ports that block has. Stated in the contract's first paragraph
rather than left to inference.

### The FOURTH phantom reference model

The ledger has now named four `reference_model` symbols that do not exist:
`zref::CmdDma`, `zref::SurfaceStamp`, `zref::SurfaceSheet`, `zref::AuxSource`,
and now `zref::TerrainBake` (the real one is `zref::terrain::bake_dig`). Rule
V17 exists to catch exactly this, and it only fires once a block's maturity
rises. Every increment that touches a new block should verify its
`reference_model` resolves before relying on it.

### Real defects the work found in itself

- `FORGE.CLIFF`'s `idx_r` was both the enumeration write pointer and every later
  read cursor and was not reset per page, so page 2 of a multi-page lattice
  wrote above its own count and emitted NOTHING, silently.
- A dropped slot field in `SURFACE.SHEET`'s write address was invisible to the
  chain at first, because every case stamped one patch and slot 0 is what a
  missing slot field decays to. A two-resident-patches case now kills it.

### The coverage lesson, now four times over

Mutations caught by the directed lane only, because uniform random input never
reaches an exact-equality boundary:

    d2 == r*r            surface stamp rim
    clip.w == 0          projection near plane
    dev == distance      the LOD ladder
    last-merge prefix    FORGE.CLIFF's shed

Each was fixed by CONSTRUCTING the boundary rather than sampling for it. This is
now in every agent brief.

### Honest shortfalls, measured not derived

    TEXTURE.AUX      6.00 clocks/sample against a target of 1
    FORGE.CLIFF      11,946 clocks for the worst page (32x32 checkerboard)
    GEOM.BINNER      2.83 cycles/bin reference against a target of 1
    TERRAIN.TESS     3.56 cycles/triangle amortised, 3.00 steady state

## Synthesis: the new RTL is being characterized

Extended `run_block_fit.ps1` with `-ExtraSources` because everything built today
sits outside the shell cone. Results so far, placed and routed against the
provisional 5CSEBA6U23I7:

    zhao_raster_earlyz    677 ALMs      zhao_raster_fill      23 ALMs
    zhao_raster_fragment  485 ALMs      zhao_raster_quant     39 ALMs
    zhao_raster_resolve   344 ALMs      zhao_raster_div255    24 ALMs
    zhao_raster_blend      53 ALMs      zhao_texture_bilerp   38 ALMs

`zhao_texture_cache` and `zhao_texture_tmu` both exceeded their budget, the same
complexity ceiling the large shell blocks hit.

### The finding that justified the whole synthesis push

`zhao_geom_binner.sv` indexed a function call's return value (`f(x)[7:0]`).
Verilator accepts it. **Quartus 17.0 rejects it outright.** That block had
directed tests, random differentials, a mutation sweep, a formal proof on its
arena, and clean `-Wall` lint, ALL GREEN, on code the synthesis tool could not
compile. Lint did not catch it either.

It also failed EVERY module in the sweep rather than only its own, because the
block fit compiles the whole source cone regardless of which entity is the top.
One unsynthesizable file looks like twenty broken ones.

This is the concrete answer to why "verified in simulation" and "synthesizable"
are different claims, and it is now in every agent brief.

## What the game design doc asks of this hardware

`untitled-game/docs/DESIGN.md` (updated 2026-08-19) is mostly pending, but its
one filled section — Haste, and its late-unlock "Wacko mode" — names two
Zhaozhou capabilities as REQUIREMENTS rather than decoration. Both change how I
would prioritise.

### 1. Live terrain deformation is a MOVEMENT effect, not a combat effect

> "Deformation waves in your wake. Ground visibly displaced along your path.
> This is the live terrain deformation the console is being built for (Zhaozhou
> phase 7), used as a movement effect rather than as combat damage."

This is a performance requirement hiding in a design note. Combat deformation is
episodic: a strike lands, a crater forms, the budget is paid once. A wake
follows a moving player EVERY FRAME, for the whole duration of the spell. The
phase-7 path (`FIELD.SEQ.EARTH` -> `TERRAIN.BAKE` -> `TERRAIN.PATCH` ->
`TERRAIN.TESS`) has to sustain a continuous stream of small deformations, not
absorb one large one.

Consequences worth checking before phase 7 is called done:
- `TERRAIN.BAKE`'s ledger target is "1 bake texel per clock"; a wake needs that
  sustained, not peak.
- `TERRAIN.PATCH`'s dirty-subpatch mask exists precisely so a small edit does
  not re-tessellate a whole patch. A wake is the workload that proves it.
- The open question in the design doc, "whether the deformation waves are
  persistent scars or heal behind you", is a HARDWARE question: persistent means
  layer B accumulates and the surface sheet grows; healing means a decay pass
  that nothing currently owns.

### 2. The player's Wacko-mode shadow IS the sun-trail law

> "A shadow of yourself streaking behind you, in the manner of the Noctis suns —
> a trailing double, not a particle ribbon."

The design doc reached for the star trail by name, and it is right that this is
not a particle system. Everything §15 was built for applies unchanged:

- a ring of retained past positions,
- stamps replayed oldest to newest into ONE six-bit intensity plane,
- decay and directional diffusion per age,
- a single class-ramp lookup at the end, which is what bounds the palette cost
  to 64 colours however much overlaps.

So `star_compose.cpp`'s trail is not a star effect. It is a general
**streaking-double primitive** that happens to have been written for suns first,
and the v1.3 work (kernel follows the heading, prominence lobe, per-age
irregularity) is directly reusable for a moving character.

What would need to change: the trail currently stamps a corona and a disc
sprite. A character needs its own silhouette as the stamp source, which is a
parameter change rather than a new law. And the six-bit plane is per-light
today; a player double would want one too, so the cost is one more plane, not a
new subsystem.

**This is worth recording in `spec/stars_and_flares.md` §15 as a note that the
law is deliberately general**, so nobody later reimplements it as particles for
the character and ends up with two smear laws that disagree.

### Consequence for the docket

The set-piece catalogue and the eclipse scene are still the right visual
targets, but this moves LIVE DEFORMATION SUSTAIN up the list: it is now a
gameplay-critical path with a named consumer, not just a phase in the roadmap.

## Scale facts from the donor, and what they mean for this hardware

Measured from the shipped *Sacrifice* data with Sacengine's parsers as the
format documentation. Full write-up in `untitled-game/docs/SACRIFICE-NOTES.md`.
Nothing was copied; these are measurements and technique.

    one world unit          ~1 metre (posed peasant 1.70)
    terrain lattice         256x256 cells at 10 units/cell, EVERY map
    world extent            2560 x 2560 units, always
    land fraction           median ~13% (range 1.9%-58.7%), rest is void
    playable island         1000-2500 units across
    wizards                 1.63-3.36 tall
    largest creature        7.8 tall, 20.7 wingspan
    camera distance         5-20 units behind the player
    relief                  median 170 units, ~1/15 of map width
    fog distance            ~0.9 * sqrt(land area), i.e. one island width

### The budget comparison, done properly

A Zhaozhou patch is 32x32 cells on a 33x33 vertex lattice (`terrain_rules §37`)
and the composed-cache budget is 256 patches (`terrain_rules §520`). The donor's
ENTIRE world is 256x256 cells = **64 of our patches**, a quarter of the budget.

But their cell is TEN METRES and ours is sub-metre by design, so the budget is
not one-dimensional:

    at 10 m cells   4x their area, 2x linear extent
    at 1 m cells    ~their extent, ~10x the terrain detail
    mixed           what TERRAIN.LOD already does at runtime

The owner's ruling: the donor's numbers are a FLOOR, not a target.

Worth noting their box is mostly empty (13% land), so raw extent is the least
interesting axis to spend on. Their flat relief (1/15 of width) is very likely a
consequence of 10 m cells rather than intent, which means sub-metre cells buy
real verticality rather than just finer ground.

### Rotated terrain sheets: the investigation to open

The owner wants the headroom spent partly on VERTICAL structure — terrain sheets
rotated to shape objects (towers, walls, cliff faces), not just ground. This is
the "rotated terrain sheets for deformable skyscrapers" idea from earlier in the
project, and it is worth taking seriously because it is REUSE:

- deformation, scars, LOD, tessellation and normals all apply unchanged, so
  buildings become destructible with no second destruction model
- layer C (the island underside, added for the deep keel) already gave a patch a
  top AND a bottom, which is closer to a vertical sheet than flat ground is

**What is already in our favour**, read from the RTL:
- `zhao_terrain_tess.sv` takes `lat_wx_i` / `lat_wz_i` PER VERTEX from the
  lattice read port rather than deriving world position from grid indices
- `zhao_terrain_project.sv` is fully matrix-driven (`mat4_vec4`, two view
  register sets)

So world position is not baked to an axis-aligned grid anywhere in the chain.

**THE OPEN QUESTION, which must be answered before this is promised to anyone:**
which axis the height layers displace along. If that is fixed to Y, a vertical
sheet needs either a per-patch axis or pre-rotated authoring. Everything
downstream looks orientation-agnostic, but that is a reading of the code, not a
test.

**Next step: prototype ONE rotated patch in the reference renderer. No RTL until
the reference proves the geometry.** That ordering is what the whole project has
been doing successfully and there is no reason to break it here.

### A correction to something I said earlier

I advised that creature textures could be tiny because a creature is ~60 px
tall. That was wrong, and the owner corrected it: the camera pushes in, you can
walk up to creatures, and Possession puts you INSIDE one, so a large creature
fills the frame. Textures must hold up at close range.

The hardware already anticipated this: the LOD ladder and the TMU's mip
selection exist precisely so the near and far cases are one asset sampled
differently.

And the palette point is worth stating clearly because it has been muddled:
**the 256-colour ceiling is a CAPTURE constraint (GIF encoding in the reel), not
a framebuffer constraint.** The console resolves to RGB565. So a close-up
creature can be rich, and additive effects transfer from the donor for free
rather than needing an index-blend LUT — which was the study's flagged open
question and is now answered.

## Split screen: I said it was undecided, and it is a shipped video mode

The owner corrected this and was right. While writing the game's mode list I
recorded "two players" as an open hardware question, listed three options
(linked consoles / split screen / shared camera), and claimed split screen
"roughly doubles geometry and command work". Every part of that was already
answered in this repo:

- `spec/video_rules.md` mode 2 is **`VIDEO_DUO`, 512x240 as two independent
  256x192 view canvases**, vertically centred in the 240 active lines, with its
  own framebuffer size and scanout law (lines 24-26, 112-119).
- `shell_duo_markers_fast` is a standing fast-lane test.
- The charter's project mandate names **local split-screen** alongside extreme
  screen-space LOD and deformable terrain.
- `FORM_LANGUAGE_HARDWARE_CODESIGN.md` §12 is titled "Split-screen is a
  source-level concept" and gives the `present duo` form with declared per-view
  budgets.

And the cost claim was wrong in the direction that mattered. Two 256x192 views
is **98,304 pixels against Z60's 384x240 = 92,160 — 6.7% more fill, not
double.** The resolution was chosen with two players in it. Geometry is not
duplicated either: §12 states that simulation, terrain fields, creature
animation and particle state are represented once and only camera-local LOD and
projection fork, and the owner's point stands that aggressive screen-space LOD
is what makes the second view cost what it is worth on screen rather than what
the first view cost.

**The lesson is the one this project keeps relearning:** I reasoned from first
principles about a question the spec had already frozen. The specs are long, but
"is this already decided" is a grep, not a derivation.

### What this hands to Phase 8, which is in flight right now

`MEASURE.GOVERNOR` is not a generic throttle. FORM §12 and charter §115 make it
the block that has to degrade two views **fairly** against declared shares
(45/45 with a 10% shared emergency reserve). That implies per-view accounting
rather than a single global pool, and a reserve neither view owns. The MEASURE
agent has been told, with the specific instruction that if it chooses a
global-only policy it must record that as a CHOSEN law with per-view fairness as
the rejected alternative — because `TERRAIN.LOD` already had to guess at this
policy once against a stub contract, and there are now two prior claimants on it.

## Digging, tunnels, overhangs: what the format already answers

The owner asked for a creature or spell that digs a tunnel that persists, and
noted round overhangs should fall out of the same mechanism. Expected to be
"very tricky with our technology". `spec/terrain_rules.md` §3 had already
settled the underlying question, in both directions.

**The model:** a dual heightfield — top (layer A base + layer B persistent
scar) and bottom (layer C, added for the deep keel) — where every column is
**one solid interval `[bottom, top]` or void**. So "can we have a tunnel" is
exactly "can a column hold two solid intervals". It cannot, and §3.6 says so
explicitly about caves, arches and through-tunnels.

**Overhangs, however, need nothing new.** §3.6 lists overhanging thin lips as
high-bottom slab columns, and rims curving up into the island — the
bitten-apple profile the §3.7 keel generator already produces by default. Round
overhangs are an authoring operation on layer C, available today.

**Four tunnel approaches, cheapest first, all read off existing law:**

1. **Trench — free today.** A digging creature is a moving stamp. The scar
   delta is already persistent, bake-written, capture-replay exact, and already
   marks nav cells dirty on change (§3.5). A deep narrow groove stays with no
   new hardware, and reads as a tunnel from this camera for most of its length.
2. **Keel burrow — also free.** Layer C is bake-writable (§3.4: the bottom
   surface changes exclusively at bake time — but it *does* change). A digger
   that raises the bottom along its path carves up into the island from
   underneath: real ground as roof, open to the sky below. On a floating island
   that is a genuinely good look for one existing layer.
3. **Breach + Wound plug — the ratified path.** §3.6 states that the void mask
   IS the Wound socket and "the patch format needs nothing further now". Charter
   §11.7 Wounds are bounded volumetric meshlet plugs parented to the island
   transform. Cost is the plug format, a Phase-11+ hero feature, not a terrain
   change.
4. **Rotated sheets — already an open investigation** (see the entry above). A
   sheet displacing along a horizontal axis gives roofs and walls from ground
   machinery. Same unanswered question: which axis the height layers displace
   along. Same plan: one rotated patch in the reference renderer before any RTL.

**A trap worth naming:** *breaching* is not digging. A breach is top meeting
bottom — a hole through the entire island — and over the §3.7 minimum 50 m keel
that is a chasm, not a tunnel. Anything that wants a roof must move layer C, not
drive layer B down to it.

**The real bill is not rendering.** The column query (§4) returns one interval,
so a Wound plug that renders correctly is still not collidable and still not
navigable. A Wound has to carry its own collision and nav or the tunnel is
scenery — and that lands on the sim side, not this hardware. Which is the
argument for shipping trenches and keel burrows first, where the existing column
query already tells the truth.

**Docket:** nothing is scheduled from this yet. Items 1 and 2 are authoring and
bake-side and need no new block; item 4 is the rotated-sheet investigation
already open; item 3 waits on Wounds.

## A build-environment trap that produced a false green, then a false red

Worth recording because it cost real time and every layer of it lied.

`tools/env/zhao-env.ps1` exists and documents the machine's rules — that
`VERILATOR_ROOT` must be `share/verilator` and not the suite root, and that **the
devkitPro msys2 cmake is first on PATH by default and is broken with native
g++**. I did not source it. The consequences, in order:

1. A bare `cmake` picked up devkitPro's, which wrote a cache whose source path
   was `/c/programmieren/...` and whose verilate rules carried
   `VERILATOR_ROOT=` empty. Verilator then looked for its own headers in
   `/yosyshq/share/verilator`, its compile-time default.
2. Every test in the fast lane reported **`BAD_COMMAND`** — which reads as "test
   binary missing", not "your environment is wrong".
3. The run reported **exit code 0**, because ctest's status was swallowed by a
   pipe into `tail`.
4. Prepending the suite to PATH by hand made it worse, not better: the suite
   ships no cmake, so the devkitPro one still won and now failed at
   `CMakeTestCXXCompiler`.

Three layers each reported success or a misleading cause. The fix was to source
the documented script and delete the poisoned build tree. Same family as the
verilate-output-deletion trap and the mtime-poisoning trap already recorded
above: **a build system that has been disturbed reports test failures, not build
failures.**


## Rotated sheets: the open question is ANSWERED, and the answer changes the shape

The entry above opened this investigation and named the blocker: "which axis the
height layers displace along. If that is fixed to Y, a vertical sheet needs
either a per-patch axis or pre-rotated authoring. Everything downstream looks
orientation-agnostic, but that is a reading of the code, not a test."

Read properly, from ratified law and the landed RTL. **It is fixed to Y, in two
places, and they are not the same kind of fixed.**

**Render side — barely fixed at all.** `zhao_terrain_tess.sv` assembles a vertex
by SLOT ASSIGNMENT, not arithmetic: `vx[p] <= lat_wx_i`, `vz[p] <= lat_wz_i`,
`vy[p] <= lat_h_i` (lines 759-762), emitted as `o_ax/o_ay/o_az`. The block never
does anything to Y it does not also do to X and Z. And `zhao_terrain_project.sv`
holds a **full mat4 view-projection matrix per view** (cfg_addr 0..15, row-major
fx16), applied as `mat4_vec4`. So an island rotation can simply be folded into
that matrix — MVP instead of VP, 16 words reloaded when the island changes — and
the tessellator, the normals block and the LOD ladder never learn about it.

**Sim side — structurally fixed, and this is the real finding.** §4.3's
`column_query(island, wx, wz)` is a 2D-to-1D function: the sparse directory is
keyed `(ix, iz)`, the patch envelope is an axis-aligned `rectfx x0,z0,x1,z1`,
and the result is ONE interval `[bottom, top]`. SW.CPUCOLL, PART.COLLIDE, nav
and the height query all go through it.

### The conclusion: rotate the ISLAND, not the patch

A patch rotated INSIDE an island puts two surfaces at the same `(x, z)` — which
is precisely the "second solid interval in one column" that §3.6 refuses, and
precisely the wall the tunnel question hit from the other side. **Rotated sheets
and tunnels are the same problem.** A per-patch axis flag would therefore break
the column law rather than extend it, and it should not be built.

Rotating a whole island keeps every internal law intact, because every law is
already expressed in island-local terms — heights are relative to the island
datum, envelopes are island-datum-relative, and `column_query` is already
parameterised by island. What changes is only the island-to-world transform.

**Cost, stated honestly:**

- §1.5 gives an island a world ORIGIN and nothing else — translation only, no
  orientation field. That is the one format change, and it is small.
- The sim's broad phase must transform the query point into island-local space
  before the directory lookup, and must be willing to test more than one island
  for the same world point. Today a vertical sheet and the ground beneath it
  are two islands overlapping in `(x, z)`, which the current query shape does
  not contemplate.
- §3.5's "falls out of the world" rule compares against
  `island_datum + min(bottom)`, which stays island-local and therefore stays
  correct — but "below" is no longer a single direction across islands.

**Next step is unchanged and now better specified:** prototype ONE rotated
island in the reference renderer — not a rotated patch — and confirm the
reference's `column_query` still satisfies the §4.3 "physics equals pixels"
differential when the query point is transformed in first. No RTL until that
holds.

**Incidental confirmation while reading:** `zhao_terrain_project.sv` already
carries two matrix + viewport register sets selected by `cfg_view_i`, with its
header naming "camera views (Duo)" and the 256x192 pair explicitly. The
split-screen path is not merely specified, it is wired into the geometry lane.


## Docket: the thick-atmosphere sun (owner: "one of the best ones, we must steal it")

Read the law before costing it, per the lesson two entries up. Most of this is
already ratified and unbuilt, one piece is a genuinely free win, and one piece
is an honest spec amendment.

**Already ours, already specified:**

- `stars_and_flares.md` §4 already has the variant. `halo_atmo`, `core16 = 0/16`,
  described in the table as "surface sun w/ atmosphere — pure glow ball (4×R)".
  `zref::star::corona_sprite(0)` bakes it and reel subject 4 already selects it.
- Fog is already the aerial-perspective term, already bound to the horizon ramp
  (`qformats.md` §8 owns the numbers), already carried per frame by
  `SetEnvironment 0x0311` with mode/near/far.
- Global tint is on the same record, applied to the LIT vertex colour before
  texture modulation — the donor's `lmap` position, tint the light not the
  albedo. That is the whole-scene wash a thick sky puts over everything.

**The free win, and it is the one that matters most.** The star ramp's control
points are `P0 = (0,0,0)`, `P1 = undertone`, `P2 = class colour`,
`P3 = (256,280,304)` — and that P3 is a **deliberate early per-channel
saturation that whitens the top of the ramp**. For a thick atmosphere that is
exactly backwards. Air thick enough to bloat the disc has already scattered the
blue out; the core should go *dimmer and redder*, never whiter.

`zref::star::RampState` holds P0..P3 as `cur[12]` in the s16 pre-clamp domain
and **slews each channel ±1 per tick toward its target**. So P3 is not a
constant — it is an animatable target. A thick-atmosphere sun is
`P3 ≈ (300, 150, 40)`: red railing early, green landing mid, blue staying low.
Zero new law, zero runtime cost, and because §4 says the corona uses **the same
ramp palette**, the halo reddens with the disc automatically — the entire glow
ball turns together.

Better still, the slew makes it a *transition*: the sun can visibly thicken and
redden as weather rolls in, which is the mechanism the eclipse and bloodmoon
set pieces on this docket already need.

**The honest gap — falloff shape.** §4's bake is a **LINEAR** falloff:
`pix = 63 − rescale_u((rr − fgm_h)·k, 16)`. With `core16 = 0` that is a straight
cone from centre to rim, which reads as a solid glow ball with a definable edge
at 4×R. What makes the donor's version good is a long soft shoulder — bright
centre, slow tail, no discernible boundary. Getting that means a new row in the
§4 variant table with a non-linear profile (a squared or reciprocal shape), i.e.
a spec amendment, not a parameter change.

It is still cheap at runtime: §4's whole point is that "the LUT *is* the
texture", so a different falloff costs bake time and nothing else. But it is a
new frozen table and it must be argued in the spec, not slipped in.

**The other gap — the sky has no azimuthal term.** `sky_and_beams.md` §1.1 gives
the sky an ELEVATION ramp. A thick-atmosphere sun brightens the sky *around
itself*, which is an azimuthal effect centred on the sun anchor, not a vertical
one. Nothing in the sky layer table expresses that today. Worth stating before
anyone promises the full look.

**Order:** the ramp-target change is free and lands with the next sun increment.
The falloff variant and the azimuthal sky term are separate, specified pieces
and go on the docket behind the hardware waves.


## The atmosphere pair, and a correction to my own correction

The owner asked for the thick-atmosphere sun rendered over an island, twice: the
donor's design and mine beside it. Both are now reel subjects, `atmo-sun-donor`
(celestial 11) and `atmo-sun-thick` (celestial 12), differing in exactly one
thing so the claim can be checked rather than believed.

### I was wrong about what the correction is

The docket entry above proposed retargeting the ramp's top control point P3
from `(256,280,304)` to roughly `(300,150,40)`. **Rendered, that is invisible.**
The pair was indistinguishable at every frame. Two reasons, and both are worth
keeping because they generalise:

1. **§4's corona falls off LINEARLY in radius**, so ramp index is roughly linear
   in distance from the centre. On a 104 px halo, the `[40..64)` segment P3
   governs covers a handful of pixels; the mid and low entries carry the entire
   visible glow. P3 owns a quarter of the ramp and almost none of the picture.
2. **The corona composites additively** (§4 `star_halo_additive`,
   `dst = sat(dst + src)`). Near the core every channel rails whatever colour
   went in, so the brightest region is white either way. Additive saturation
   eats precisely the correction P3 was making.

**What actually works is a transmission filter.** Thick air attenuates per
wavelength at every intensity, not only at the top, so the honest model is a
per-channel multiply — `(1.0, 0.60, 0.25)` — applied to the control points P1,
P2 and P3 before §3 builds the ramp. P0 stays black; black is black through any
depth of air. §3's segment law and its single rounding are untouched, only its
inputs change, and it costs three multiplies once at bake time.

That version reads immediately: a deep orange disc inside a warm glow against a
dark island, where the donor variant is a pale disc in a white glow.

### What the palette law forced, and it was not a compromise

First render: **395 and 408 unique colours against a ceiling of 256.** The cause
is the sequence palette counting the union over every frame, so a descending
corona multiplies its own falloff levels by every sky row it crosses.
`sky_variant = 1` only flattens the UPPER band, which is enough for subjects
whose additive chain stays high and not enough for one that sets through the
horizon. Added `sky_variant = 2`, a fully flat sky (trivially C0 under §1.2 —
every join is an equality). Result: 151 and 152 colours.

The flat sky is deliberately **near-neutral** `(110, 96, 104)` rather than a warm
dusk. These subjects exist to compare a whitening ramp against a reddening one,
and a warm sky would flatter the reddening one before the comparison started.

The island is flat-shaded for the same additive reason flare-occlusion is, and
that is the right picture anyway: dark land under a vast glowing sky is the look
being argued for.

### Honest limits of what was published

- Still the **linear** falloff. The soft-shouldered profile the donor's version
  actually has remains a §4 table amendment and is not in these renders.
- Still **no azimuthal sky term**, so the sky does not brighten around the sun.
  In these frames the corona IS the sky glow, which is a stand-in, not the
  effect.
- The transmission constants are authored, not derived from any scattering law.
  They are a look, chosen and stated.

CRCs pinned at first render: donor `0xD16723F6`, thick `0x08B5A606`.

## The deleted captures were the fast lane's only red

Restored `captures/failures/` from HEAD and `field_crater_ring` went green
immediately; the full fast lane was otherwise 162 of 163 with `format_check`
skipped (no clang-format on this machine).

The failure mode is worth recording. `test_field_crater_ring` gate 6 compares a
committed failing vector against a freshly minimized one, and writes the pair
only when it is absent. With `captures/failures/field/` deleted *including its
`.gitkeep`*, the directory itself was gone, so the test could neither compare
nor write, and reported `gate 6: failing vector + report WRITTEN (first run)`
as a FAILURE. Deleting committed evidence therefore does not merely lose
evidence — it takes a lane red in a way that reads like a code defect.


## Pushed, and the Duo frame answered

**Pushed** at `d18053c`: eleven commits, origin/main up to date, nothing
outstanding. Gates run before the push, all green and all named rather than
asserted:

- `ctest -L fast`: **100% of 170 tests passed**, `format_check` skipped (no
  clang-format on this machine). That set includes the five new MEASURE tests
  and both MEASURE lint gates.
- `npm run ledger:check`: 88 blocks / 40 ops, schemas + V1-V17 + V19-V20 +
  staleness green, 19 formal runs recorded.
- `npm run abi:check`: 26 outputs match. `npm run tables:check`: 10 generated
  files byte-identical.

`measure_governor_lod` passing is the one worth calling out. That is the
composition against `TERRAIN.LOD`, the block that had to CHOOSE a governor
policy against a stub contract. It composed rather than contradicted.

### The owner asked why the two Duo views show different amounts

They do, and it is deliberate. `tests/render/render_golden.cpp:82-86`:

```
// camera (w = z + 40, eye 14 m) so the two Duo views differ
const int32_t eye  = view == 0 ? 12 : 14;
const int32_t camz = view == 0 ? 32 : 40;
```

Both viewports are 256x192 and the projection scale is `2<<16` for both, so
the field of view is identical. Only eye height and camera distance differ,
which is why the left island reads closer.

**Why it should stay that way in the test:** if the two views were ever wired
to one camera, or one view's state leaked into the other, two matching pictures
would look entirely correct and the fault would pass unnoticed. Disagreement is
the detector.

**Why the game must do the opposite:** the design doc's rule is equal
visibility for everyone, because a higher viewpoint is a competitive advantage.
The golden frame violates that rule precisely because it is not gameplay. The
two ideas must not be confused, and the site caption now says so — it previously
said "from different cameras" without saying why, which reads as an unfairness
that is not there.

Site redeployed with the corrected caption and the atmosphere pair.


## Phase 8: the composition GEOM.BINNER waited two phases for

`MEASURE.TOKENS x GEOM.BINNER`, both blocks real, landed at `994c5b9`. This was
the increment the phase-8 agent named as most valuable for that block and did
not do, and it is the seam `GEOM.BINNER`'s law E deferred in phase 6 with the
words: MEASURE.TOKENS "is phase 8, its contract is still a stub, and no packet
layout for `token_grant` exists anywhere".

**How the two blocks meet.** Law E says `tok_grant_i` "is sampled on that same
edge", so the composition has to honour a combinational round trip inside one
cycle. A new `BinnerDev::token_authority` / `after_edge` pair opens exactly that
window: after `tri_ready_o` rises (so `tok_req_o` is genuinely asserted) and
before the accepting edge, the authority drives the real TOKENS instance, reads
`tok_grant_o` after a single eval, writes it back, and both blocks then take the
same edge. A single eval settling is itself the evidence that no combinational
loop crosses the seam.

**Measured: 6 granted, 4 denied against a budget of 6**, the binner's own
`triangles_culled` up by exactly 4, and view 1's pool untouched throughout —
charter §9 fairness observed from the CONSUMER side rather than from inside the
block that implements it.

### The seam is six bindings short, and that asymmetry is the finding

`MEASURE.GOVERNOR → TERRAIN.LOD` composed with **no adapter at all**. This one
needs six decisions nobody has ratified. `src_id` and `class` are sound;
**`view` has no port on the binner at all**; `cost`, `essential` and `rep` are
CHOSEN in the test because nothing supplies them.

Two consequences now stated rather than left implicit:

- **`den_rep_o` reports 0 for every denial from this producer**, whatever LOD
  rung actually made the triangle. It is useless for the binner today.
- **Law T1's essential/refinement reserve has no producer exercising it.**
  Nothing upstream of the binner marks a triangle as refinement.

And one constraint discovered rather than assumed: **a per-tile-reference cost
model is not buildable at this seam.** The reference count is not known until
the triangle has been enumerated, which happens after the grant. A cost model
that needs the answer before the question cannot exist here.

### REPORTED DEFECT: the pool only ever drains

TOKENS publishes a return path and law T4 gives it meaning. **GEOM.BINNER has
no return port.** Law E named the return path as one of four things it left for
this block to write; this block wrote it and gave the binner no way to reach it.
A granted triangle's token is gone for good, so the geometry pool falls
monotonically and after `budget` triangles that producer is denied forever
however much of the work finished.

The test asserts `avail_geom0_o == 0` — pinning the defect AS IT IS, on purpose,
so that wiring a return path turns the line red and forces whoever does it to
state the new law here rather than fixing it quietly. Adding a port to a landed
phase-6 block is its own interface change; proving the gap exists is the honest
first step.

### A fixture trap worth keeping

Screen coordinates into `make_bin_tri` are **8.8 subpixel**. Plain pixel numbers
made every triangle sub-pixel, GEOM.CLIP rejected all ten, and the frame ran
empty. The fixture assertion ("ten triangles survived CLIP and SETUP") is what
caught it — without that line the composition would have "passed" against zero
triangles, which is the same failure shape as a green mutation sweep against a
stale binary.

**Gates:** fast lane **100% of 171**, up from 170 by this test. The 16 related
lanes including every pre-existing binner test are green, so the driver hook
disturbs nothing.


## Phase 8 reaches synthesis, and the characterization report was lying twice

Both landed phase-8 blocks now have real fitter numbers against the provisional
5CSEBA6U23I7. The phase-8 agent left them "simulated, not synthesized and not
on hardware"; that half is now closed.

| block | ALMs | registers | fit |
|---|---:|---:|---:|
| `zhao_measure_tokens` | **1,422** | 664 | 749 s |
| `zhao_measure_governor` | 589 | 486 | 587 s, reproduced at 763 s |

**`MEASURE.TOKENS` is now the LARGEST block in the design**, ahead of
`zhao_geom_binner` at 1,303. The charter §9 fairness guarantee costs more
silicon than the binner it guards. That is not obviously wrong — five 32-bit
pools with saturation and a shared-reserve path — but `TOK_W` is the lever and
nobody has argued for 32 bits. Worth settling before any device is frozen.

The governor reproducing 589 exactly across two runs is worth noting on its own:
the measurement is stable even though the wall time was not (587 s vs 763 s
under different machine contention).

### DEFECT 1: the report overwrote instead of merging

`run_block_fit.ps1` wrote a report containing ONLY the modules of the current
invocation. A two-module run turned the committed **21-block** report into a
**2-block** one — nineteen blocks of fitter evidence gone from the working tree,
recoverable from git, with nothing anywhere saying it had happened. Same shape
as today's deleted captures: evidence disappearing while every exit code says
success.

Fixed: the script now loads what is on disk, replaces only the rows this run
measured, keeps the rest, sorts by module so diffs show measurements rather than
moved lines, and PRINTS the arithmetic — `WROTE ... (23 block(s); 1 measured
this run)`. That message is the proof the merge happened.

### DEFECT 2: ten rows said `timeout` and meant "we did not wait"

`zhao_measure_tokens` reported `timeout` at the script's 900 s default, then
fitted cleanly in **749 s of quartus_fit** on the next run with a larger budget.
So `timeout` never meant "this block does not fit". It meant the budget expired.

**Ten of the twenty-three rows carry that status**: `texture_tmu`,
`texture_cache`, `terrain_tess`, `terrain_project`, `terrain_patch`,
`terrain_lod`, `surface_sheet`, `raster_edgewalk`, `geom_setup`, `geom_clip`.
Every one is suspect for exactly the same reason, and the report as committed
reads as "twelve fit, ten are problems" when the truth is "twelve fit, ten are
UNMEASURED".

Default raised to 3000 s with that measurement recorded as the reason, and all
ten queued for re-characterization. A default that manufactures false failures
is worse than a slow one, because a false failure gets designed around.


## Planetside suns: the donor's actual technique, and why the first attempt could not work

Ten worlds are live on the site. The owner's correction was blunt and correct:
the atmosphere pair I published earlier "is not what I was talking about", and
the suns in it "might be fireball effects for spells and stuff". They were: a
coloured disc with a halo, which is a SPACE sun wearing warmer paint.

### What the donor actually does, read out of its own source

`noctis-0.cpp:2873` `white_sun()` rasterises an **additive, linearly-falling
radial splat directly into the sky's six-bit intensity plane, before the terrain
is drawn**, and the atmosphere is ONE number: `K=4, fgm_factor=0` with air (no
core, no disc at all), `K=3, fgm_factor=0.5` without. Two consequences make the
whole effect:

1. **The sun has no colour of its own.** It saturates the plane to 63, and 63 is
   the sky palette's own peak entry. "Different planet, different sky, different
   sun" is ONE mechanism, not two — which is exactly what the owner said about
   islands and skies before any of this was read.
2. **The formless bloom is emergent from additive-plus-clamp.** Where the sky
   under the splat is already bright, the sum rails over a wide area IN THE
   SKY'S OWN COLOUR, so the bloom has no edge anywhere. Composite an
   alpha-blended sprite over a background instead and the effect cannot happen
   at any radius or any softness of falloff. That is why the first attempt was
   not fixable by tuning it.

Full study in `untitled-game/docs/NOCTIS-SURFACE-NOTES.md`.

### What landed

`planet_sky_hook` in the reel builds the sky as a six-bit plane — vertical ramp
(brightest at the horizon, the donor's `crcy = s_background[p] * cpos /
bk_lines_to_horizon`) plus two octaves of PCG value noise standing in for
`nebular_sky`'s smoothed middle-square fill — adds the sun splat INTO it, clamps
at 63, and maps through one 64-entry ramp per world. Ten worlds in `kPlanets`,
each six colours and four numbers.

**The palette result is the headline.** Every loop carries a full gradient sky,
cloud mottling and a sky-filling bloom, and they measure **93 to 156 colours**.
The earlier atmosphere pair had to be given a FLAT NEAR-NEUTRAL sky to fit at
all, because an additive sprite over a gradient multiplies halo levels by sky
rows. Sharing one plane and one ramp removes that product entirely.

And the proof that it is the technique rather than luck: **the 480 px red giant,
the largest sun in the set, produces the loop with the FEWEST colours (93)** —
against 156 for the blue supergiant. A sun that saturates most of the frame
collapses onto one entry. Bigger is cheaper.

### Two defects found by looking at the output

- **The RGB sky dome was still being drawn underneath.** Its under-plane sat on
  screen as real geometry with a depth, and the hook correctly skipped it as a
  surface — a flat salmon band under the island belonging to no planet's ramp.
  Planet subjects now suppress the `DrawSky` record entirely.
- **The binary railed the entire frame white.** Two 300 px and 150 px blooms
  over a base of 24 leave nowhere unsaturated, and two suns you cannot tell
  apart are worse than one. It got its own darker world with two smaller,
  well-separated stars, which keeps both blooms readable AND the dark band
  between them.

### Honest limits

The vertical ramp falls away BELOW the horizon as well as above it, which the
donor does not do — it stops at the horizon because you are standing on ground,
and here the world is a floating island with open sky underneath. That is our
choice, not the donor's law, and it is why these read as islands hanging in air.

There is still no azimuthal sky term: the sky does not brighten around the sun
independently of the splat. In these frames the splat IS the sky glow, which is
now much closer to correct than the corona stand-in was, but it is not the same
thing as a scattering term.

All ten CRCs pinned at first render; every GIF verified byte-exact on decode.

