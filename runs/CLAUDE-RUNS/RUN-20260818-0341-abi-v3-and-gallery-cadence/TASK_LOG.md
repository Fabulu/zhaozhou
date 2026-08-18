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

### Open decision: black does not resolve to black

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
