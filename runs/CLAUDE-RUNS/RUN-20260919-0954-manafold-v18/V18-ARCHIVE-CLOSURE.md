# Manafold version 18 — Wave A archive closure

**Date:** 2026-09-19
**Direction:** Owner Direction 19
**Architecture:** `V18-ARCHITECTURE.md` §9 / Wave A
**Verdict:** **PASS — version 17 and both experiment collections are immutable and archive-only**

## Immutable byte archive

Before any version-18 encode can overwrite live names, every production-verified version-17 Manafold file was copied to `archive-v17-manafold-*`:

- 28 WebMs + 28 posters;
- **56/56** source/archive pairs match exact SHA-256 and byte length;
- **70,576,645 bytes** total;
- source generation: Upheaval `664f415`, Zhaozhou source `18e1d993`, renderer MD5 `67DCAFF8ABC0AA2BA830C439A4CD98C7`.

The pairwise receipt is `V17-ARCHIVE-SHA256.txt` in this run and beside the creature in Upheaval. The latter is consumed by the permanent site gate.

## One version-17 experiment generation

`creatures.json` now has one archive generation labelled exactly:

`Version 17 + mana experiments — 2026-09-19`

It contains three collections:

1. `Version 17`: exactly 22 non-menu production clips on immutable archive paths;
2. `Mana menu`: exactly six clips on their immutable `archive-v17-manafold-mana-*` paths;
3. `Mana lab · 2026-09-05`: exactly ten clips retaining their existing lab paths.

Every version-17 production clip is declared exactly once: 22 + 6 = 28. Menu clips are not duplicated in the general collection. Menu and lab no longer consume live tabs. The archive introduction now says thirteen generations and identifies version 17 plus its experiments as the newest.

After archive hashes and declarations passed, the twelve now-undeclared live menu WebM/poster files were removed. Their history was not deleted: the declared immutable copies are byte-identical. The source-frame directories remain in the current scratch bank and are reported as explicit archived freshness exemptions until the version-18 22-subject bank replaces that junction.

## Permanent gates

New committed `website/tools/checkarchive.py` is wired into every assemble/deploy. It verifies:

- all 56 archive files against the locked digest receipt;
- live source/archive pairs where the live source still exists;
- exactly 22 + 6 + 10 declarations in the generation;
- no duplicate media path;
- Mana menu and Mana lab absent from live tabs;
- archived video playback is controls + loop + muted + playsinline, without autoplay, with `preload="none"`.

Its selftest passes a valid fixture and fires three red legs: live menu, duplicate declaration, and broken archive playback.

`checkfresh.py` now classifies rendered sources using live/archive manifest ownership. It continues to fail stale, absent or unknown live source, while explicitly exempting only version-17 subjects locked by `archive-v17-*` declarations. Its selftest fires on an unmanifested rendered subject.

## Complete local gate

`deploy.ps1 -Project upheaval -Branch main -AssembleOnly` ran with no skip flags and the installed FFmpeg 9.0.1 directory on `PATH`:

- assemble: 2 creatures / **716 render entries**;
- live Manafold outer tabs: **22**;
- archive: **13 generations**;
- version-17 archive: 56/56 locked files, 70,576,645 bytes;
- version-17 declarations: 22 general + 6 menu + 10 lab, unique;
- playback: live Fall controls-only/non-loop; live Hover autoplay+loop; all 38 newest-generation archive videos controls+loop/no-autoplay/preload-none;
- robots: exact `noindex, nofollow`;
- freshness: **22 live fresh / 6 archived / 0 stale / 0 absent / 0 unknown**;
- media decode: **1,420 declared / 1,420 decoded**;
- overall RC: **0**;
- deployment: **none** (`-AssembleOnly`).

Generated `website/public/index.html` is 425,157 bytes, SHA-256 `86626fcc13ae29fcc8c5df81111b8ad4becb00316bf27dd22a50f841b75720f7`.

Durable command output: `v18-archive-gates.log`.

## Changed paths

Upheaval:

- `creature/Manafold/V17-ARCHIVE-SHA256.txt`
- `creature/Manafold/VERSION-18-INVENTORY.md`
- `creature/Manafold/VERSION-18-PLAN.md`
- `website/creatures.json`
- `website/deploy.ps1`
- `website/tools/checkarchive.py`
- `website/tools/checkfresh.py`
- `website/public/index.html`
- 56 new `website/public/renders/archive-v17-manafold-*` files
- 12 removed live Mana-menu media files

Zhaozhou receipts:

- `V17-ARCHIVE-SHA256.txt`
- `V18-ARCHIVE-CLOSURE.md`
- `v18-archive-gates.log`
- `TASK_LOG.md`

## Disposition

Wave A is closed locally and ready for its independent commit/push. No production renderer source, version-18 media, main branch or deployed site changed.
