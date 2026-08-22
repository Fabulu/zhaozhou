# Public gallery creature repair

Completed: 2026-08-18
Scope: `zhaozhou` creature authoring/reference repair and `zhaozhou-site` publication only.

## Diagnosis

The old GIFs decoded byte-for-byte to their raw frames, so encoding was not the fault. Bone-local ring vertices had not been moved to their bone world-rest positions before rigid `A * inv_rest` skinning, piling every body part at the root. Combined quarter turns also used the opposite order from the documented authored convention. The watchdog was consequently an end-on barrel. The old pop rendered tiny screen squares and advanced excessive velocities on the spawn frame, leaving background-only frames after breakup.

Before evidence:

- `evidence/before-creature-wave-walk.png`: SHA-256 `701ec3ee904d4ee707b1e245f82f67523fcab2e3868bd6e1ec05ca5a44087e50`
- old walk GIF: 1,223,213 B, SHA-256 `d3a57a32265aefe06e2897c0efaa7da40be3ad1a276fdc0faae03ee776d27724`
- `evidence/before-creature-bulk-pop.png`: SHA-256 `86ce925a1cf3766c7655a88a468c687df7fba68bfadc3e6910f388602a082c86`
- old pop GIF: 30,748 B, SHA-256 `4bc355e38ba23788169bcbf5390dc23652a340564d33ea073b641dff9317e346`
- old pop frames 40-71 were identical background-only frames.

## Repair

- Corrected ring quarter-turn composition to authored `Ry * Rx` order.
- Compiled full and micro rigid meshes into creature-global bind space at each assigned bone's world-rest attachment.
- Added focused axis-order and child-bone bind-attachment tests.
- Re-authored the watchdog as six rigid parts: long torso, pale head, and four separately bound legs.
- Authored movement along +X, diagonal gait around Z, front-quarter facing, root reach compensation, and head nod.
- Replaced reel-only gib squares with 18 independently translated, rotating, depth-tested cube chunks sourced deterministically from donor gibs.
- Added per-frame integer gravity, damped ground bounce, lifetime removal, and a held breakup frame.
- Added runtime invariants for spawn count, above-ground starts, early in-view count, and increasing fragment span.
- Added both creature subjects to `zhao-reel --check`, expanded provenance capacity, and pinned the repaired sequence CRCs.
- Updated the truthful catalogue, site alt/body copy, and representative stills (walk frame 0; detached-chunk frame 42).

## Visual inspection

- `evidence/after-creature-wave-walk.png`: SHA-256 `78aa8c0775bafd51b0bff81b3e2ccc01afd18b409013b820f33ab34cf2c03385`. Near frames visibly show a long rust torso, distinct pale forward head, four leg attachments, alternating stride phases, and terrain-following tilt. Pullback frames visibly traverse mesh/micro/splat/glint evidence.
- `evidence/after-creature-bulk-pop.png`: SHA-256 `07457225b5127932f3675d116f31d26c9471371deebf16c52a52f834a29cfcc0`. Pre-break frames read as the same quadruped. Frame 39 visibly replaces it with detached shaded geometry; subsequent frames show chunks rotating, separating, arcing, contacting the hill, and expiring.
- Pop invariant: 18 spawned; 12 early frames with at least 10 centres in view; fixed-point span `133018 -> 980452` raw.

## Determinism and assets

Both subjects were rendered twice from the same clean source overlay. Every raw frame, palette, and metadata file was byte-identical between runs.

- walk: 98 files/run; framed manifest SHA-256 `df76ec862211f4e9cd5c339c1ff8beea2f804985b2a089a18e24adef7dfc266b`; 96 frames; palette 232; sequence CRC-32C `0x82442994`
- pop: 74 files/run; framed manifest SHA-256 `0e9696dc464bdef539adc0c3ea6a06fd08e84433256b8869bb83b5339666a402`; 72 frames; palette 160; sequence CRC-32C `0x9E32172C`

`togif.py` encoded each run independently with `paletteuse=dither=none`. Both decodes matched every RGB source frame byte-for-byte, and the two resulting GIFs were byte-identical:

- `creature-wave-walk.gif`: 1,258,587 B; SHA-256 `0863de219a498edd6dcde2993ed3b1cef2093e5b95eb63cb94dd3c44df8fd0a2`
- `creature-bulk-pop.gif`: 104,236 B; SHA-256 `7ba2644e88bcfdb85ea7b2acdad1d396e9373a5dad5512003f90a5a68e466362`

## Verification

- `test_creature_core`: pass
- focused CTest `creature_core|reel_sequence_crc`: 2/2 pass
- relevant renderer CTests: 6/6 pass
- `zhao-reel --check`: all subjects pass, including both new creature CRC pins and detached-piece invariants
- complete clean-export fast CTest: 87/87 pass
- pinned LLVM 15 format gate: pass
- cppcheck warning tier: pass
- effects catalogue YAML/statistics load: pass
- site assembly and copycheck: pass

## Commit and CI

- commit: `70421e1e269bf02923c58e9d204b07fe37dd2942` (`Repair creature gallery geometry and gibs`)
- `HEAD == origin/main == remote main`: `70421e1e269bf02923c58e9d204b07fe37dd2942`
- required push CI: success, https://github.com/Fabulu/zhaozhou/actions/runs/32084856051
- successful jobs: format/static analysis, CMake/fast CTest, npm ledger/fixgen/ABI
- scheduled nightly/formal push job: skipped by workflow policy

## Deployment

Deployed only through `C:\programmieren\zencrifice\zhaozhou-site\deploy.ps1`.

- deployment: https://33e8e7fb.zhaozhou.pages.dev
- canonical: https://zhaozhou.pages.dev
- canonical `index.html` and both repaired GIF/PNG assets were fetched after deployment and proved byte-identical to local `public/`.

## Preservation

All 43 pre-existing Zhaozhou status records match the baseline byte-for-byte, including status code, index entry bytes, worktree bytes, size, and path.

- baseline manifest SHA-256: `02e72e37ad6540cc5d66468da6f9c748c356f30f3f9ad659db7d0481a79b712a`
- final manifest SHA-256 (same records, advanced HEAD): `06de39d7a6dedf087daa58dbf810d168908501cbe0ffb10b558313bc933b6811`
- Nanquan remained clean at `d1048f8f9c46523882945e2839c2e8efebbdc80d`.
