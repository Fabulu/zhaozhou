# Manafold final4 exact-generation integrity

**Renderer binary MD5:** `1255A8F8DEE778F7E76DCC7759D678B0`
**Accepted binary path:** `runs/CLAUDE-RUNS/RUN-20260916-1838-manafold-pass16/evidence/d13-build/bin/zhao-reel-cel.exe` (digest re-verified 2026-09-18)
**Frame root:** `C:\programmieren\zencrifice\manafold-p16\final4-reel`
**Sheet root:** `C:\programmieren\zencrifice\manafold-p16\final4-sheets`

A 2026-09-18 filesystem/receipt check required, for every subject: matching `subject=` name; declared `frames=` count; contiguous zero-based `.rgb` names; one `frame_*_crc32c` row per frame; matching PNG sheet present and newer than its newest source frame.

**Result:** PASS — 28 subjects, 11,592 frames, 28 sheets, 0 errors.

| Subject | Frames | Sequence CRC32C |
|---|---:|---|
| manafold-blown | 292 | `0xA27AAFE7` |
| manafold-channel | 420 | `0x915E343F` |
| manafold-crackle | 600 | `0xDC1A02E2` |
| manafold-curious | 180 | `0x19A533FE` |
| manafold-damage | 464 | `0xED875A0B` |
| manafold-death-drop | 450 | `0x6F676738` |
| manafold-death-gutter | 590 | `0xBC8EE100` |
| manafold-drift | 300 | `0x03AD63A2` |
| manafold-fall | 340 | `0xAB4A3B05` |
| manafold-flight | 352 | `0xA00600FF` |
| manafold-hasty | 240 | `0x30553DCD` |
| manafold-hit | 140 | `0x24122E5A` |
| manafold-hover | 600 | `0x571C3D79` |
| manafold-inspect | 600 | `0x3C083851` |
| manafold-lasso | 336 | `0x88B892BD` |
| manafold-mana-aqua | 600 | `0x7136ABBB` |
| manafold-mana-blue | 600 | `0x2321B94F` |
| manafold-mana-boil | 600 | `0x96055A33` |
| manafold-mana-cyan | 600 | `0x20844E14` |
| manafold-mana-green | 600 | `0x9C651B63` |
| manafold-mana-stack | 600 | `0x8BC5FEE9` |
| manafold-pirouette | 240 | `0x20D48C9C` |
| manafold-rest | 400 | `0xF325D6C2` |
| manafold-startle | 160 | `0xBAB16BFA` |
| manafold-taunt | 280 | `0x86C357D2` |
| manafold-taunt2 | 240 | `0x7431CB1B` |
| manafold-taunt3 | 368 | `0xC0308285` |
| manafold-trick | 400 | `0xF865F644` |

The first ad-hoc check looked for `frame_*.rgb` and therefore falsely reported zero frames; the renderer names raw frames `0000.rgb`, `0001.rgb`, etc. The corrected check used `*.rgb` plus a strict numeric-stem/contiguous-index requirement and is the result recorded above.
