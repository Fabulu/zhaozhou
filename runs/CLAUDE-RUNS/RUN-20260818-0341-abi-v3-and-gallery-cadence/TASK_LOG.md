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

### Open: big suns smear forward and sideways, should trail only

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

Renaming `FORM_LANGUAGE_HARDWARE_CODESIGN.md` is a judgement call: it is a
historical co-design document with a provenance header. Retitling the content
to Nanquan while keeping a note that it was written as "Form" is honest;
silently rewriting history is not.

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
