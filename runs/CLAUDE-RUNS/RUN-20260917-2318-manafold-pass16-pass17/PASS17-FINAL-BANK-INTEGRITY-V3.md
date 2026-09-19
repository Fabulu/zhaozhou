# Manafold Pass 17 exact final-bank integrity v3

**Date:** 2026-09-19
**Status:** **PASS — Trick-repaired exact bank and every-frame sheets are complete**
**Tracked HEAD:** `b6495e1e82bf17eeaabf90291ba5712e24210a14`
**Accepted source commit:** `18e1d993`
**Branch:** `manafold-pass17`
**Environment:** `ZIXX_EXP=celmain`, `ZIXX_LIGHT=diagonal-cool-cross`

This is the sole current shipping-bank integrity receipt. Fresh isolated review in
`PASS17-FINAL3-TRICK-REVIEW.md` accepts all 400 current Trick frames; the other
27 subjects transfer through the exact byte-equality receipt below. The combined
current-bank visual verdict is **PASS 28/28**.

## Rejected-bank history

Two earlier exact banks remain historical failure evidence only:

- renderer `3390F2B8214473FEBFA32092A98E9056`, manifest `560c1237...`:
  Death Drop's soft-opaque folded backing cut off at f0234;
- renderer `6FD6147B43056E823F0487F9DFBB40BA`, manifest `7dbc84e2...`:
  both deaths were repaired, but Trick's yaw-zero pure-X hold kept both eye
  plates edge-on and failed exact-bank batch 07.

Neither bank may be encoded or cited as current acceptance.

## Exact build and invocation

A fresh caller-owned output was built at `.tmp/p17-final-bank-v3-build`; no
prior object or executable was reused.

- clean direct `cel` build: RC 0;
- renderer: `.tmp/p17-final-bank-v3-build/bin/zhao-reel-cel.exe`;
- renderer MD5: `67DCAFF8ABC0AA2BA830C439A4CD98C7`;
- renderer SHA-256: `D60EBA80C2547032A603365837690FB14B551D90D96FB5973F1C976C185C3FE9`.

The same hashed binary received all 28 canonical Manafold subject names in one
process. Renderer RC: **0**.

Current raw root:

`C:\programmieren\zencrifice\manafold-p16\pass17-final3-reel-28`

## Raw-bank validation

The committed `tools/reel/rgbframe.py` reader validated every frame header,
384x240 dimensions and exact byte count. The manifest check also required the
exact canonical subject set, matching metadata, declared clip lengths,
zero-based contiguous filenames and one contiguous per-frame CRC receipt row
per raw frame.

**Result:** 28 subjects, **11,592 frames**, **3,205,048,896 bytes**, zero errors.

Bank-manifest SHA-256:

`bc2d4d0bdce632815f42cca208763fb04bca62034e285ed1ef707c7f28d55d09`

It hashes UTF-8 rows in sorted-subject order with tab-separated subject, frame
count, renderer sequence CRC32C, ordered-frame-byte SHA-256 and byte count.

| Subject | Frames | Sequence CRC32C | Ordered frame bytes SHA-256 |
|---|---:|---|---|
| manafold-blown | 292 | `0xFA396BEE` | `4369b638013e9de534eedaaae6ddafc7214ac7d91fcc9d7e41ba6b897e643900` |
| manafold-channel | 420 | `0xB26245C8` | `51801ec5ae51050f2f272670e33ed0483025dbd9037d61bdbebec057abe8fcf2` |
| manafold-crackle | 600 | `0x5232CEDF` | `c3b1296ce4424f84c8c3e70470882b37d45c35a85907b8cec461163ff0dfb0b2` |
| manafold-curious | 180 | `0xEA31677C` | `fd77bf220a1e89ce3988c6b0a35e2e141f3daaf633dbf03d42b74ee8e7a79947` |
| manafold-damage | 464 | `0xF6FFFAF1` | `74db58396b1f2e259bf88a926f1e29de81f37b16eb6efd27885bc2d4060ca83a` |
| manafold-death-drop | 450 | `0x4A8CE910` | `21660dd9985c90b5a92e164c60a16d70919733c1a5f7dd325a31dfadda75ef96` |
| manafold-death-gutter | 590 | `0x1D892126` | `1ed05b36600c4a7928864870875cc9bbed7f53ff81b5eaea30e333b7074c03ca` |
| manafold-drift | 300 | `0x830E581D` | `39e441419abf6df44deb9a2aa34f814246212ff37bde4970f3d4a1846ac1c770` |
| manafold-fall | 340 | `0x1A7047B3` | `3c171bb42a2e8bd5326ad75aec6bfa3f35d521532666d5806938573493138782` |
| manafold-flight | 352 | `0xF2D4BAC5` | `7093a03dbca5d4c116647d4fd0d7de764faef82aa54bb202de51ec8489c0cc3a` |
| manafold-hasty | 240 | `0xC3431A63` | `e14c8f03a54731109a9a435ce5c48f4a207be5f655dc5d1a6fb35bc4094a8017` |
| manafold-hit | 140 | `0xBE8B6918` | `898ce8ffb79639b59e3955551dffa0f36790bdce9c2c464aeb3829687e5638bb` |
| manafold-hover | 600 | `0x5BBBC743` | `be6f67bd3c46b8ecbf171c7242435b74e74c0ac7b709bc22d84769a58930e076` |
| manafold-inspect | 600 | `0x60C70684` | `968708746842f84fe186f1af93fed6b61627c4a28a12a0a98d11791c1e63abe2` |
| manafold-lasso | 336 | `0xA57C140E` | `05da140d18dd4d05c85fb1aff158d72063fe1357ae167ab6d1d903dbf52ee08e` |
| manafold-mana-aqua | 600 | `0x0FCD3FD9` | `8895725ec43734fdc52f8fa198987e4169b772977728e0d267c2d1ea016ed96f` |
| manafold-mana-blue | 600 | `0xB17DB1EA` | `84821bf0edb06f67fdc3b50ea914ea00272db29cae52aaac701e732423a005ef` |
| manafold-mana-boil | 600 | `0x08954AAD` | `b9fe790d5bcefaaafc15e2906ced54d48c5cadfc6d77b0d73655f53167594e70` |
| manafold-mana-cyan | 600 | `0x2C885153` | `00c898bed07212e5839070232ec8554337d351dbdee9c23ca51703f1017c885a` |
| manafold-mana-green | 600 | `0xE813A97B` | `ca4017b752f021c41109b342c01c9aa074fd34e647450cbe29e26bfe55be275c` |
| manafold-mana-stack | 600 | `0x19B5F3A8` | `198b689c0d921cd20925d43fab89f80f6c8922e69709feb1231c604fc4b4fdf9` |
| manafold-pirouette | 240 | `0xE5D139FD` | `d6807d76f26ee128dffa5dac5883829d4dcfb0b2bd1f9ebb8feda38cb91b70c1` |
| manafold-rest | 400 | `0x268F219E` | `1aeb56c25d31998f7a64ea256d5138718c329df7f9af995b54c404c639440cdd` |
| manafold-startle | 160 | `0x750FFF50` | `fef050ef3d140ec9912e7f313462175af2309c0adeda0f95d3dd55d3fd3e35c7` |
| manafold-taunt | 280 | `0xB64E1088` | `f3b61532fad8b0e12a12938b0dc5176d43ebb1ac9588e19f8627fc5654d68892` |
| manafold-taunt2 | 240 | `0x54AAA53A` | `eca34f617591927c65aedab98b1b43666ecd19225e57a338465c847e6ff98321` |
| manafold-taunt3 | 368 | `0x618277B7` | `f5f9af8efe6e3a8554cb09765f3e287248ba173b5d748dc14134c042b23f3a13` |
| manafold-trick | 400 | `0x0BB73CDE` | `33d6146db8413ca467e25ac641fca7227b9afc2339679759d7a20bf10ed9ae2e` |

## Exact v2/v3 scope proof

Every ordered frame was compared byte-for-byte against the rejected v2 bank at
`pass17-final2-reel-28`.

- **27 subjects are exact:** 11,192/11,192 frames byte-identical;
- **only Trick changed:** no unexpected subject or frame changed;
- current Trick is 400/400 byte-identical to the focused reviewed +16384 ladder
  candidate, sequence CRC32C `0x0BB73CDE`, ordered-frame SHA-256
  `33d6146db8413ca467e25ac641fca7227b9afc2339679759d7a20bf10ed9ae2e`.

The complete v2 visual verdict for all 27 non-Trick subjects therefore transfers
exactly. This equality proof cannot choose Trick's art; its full current sheet
still requires fresh isolated review.

## Every-frame contact sheets

`tools/reel/plates.py` generated one sheet per subject with nearest-neighbour
4x downsampling (`scale=-4`) and 20 columns at:

`C:\programmieren\zencrifice\manafold-p16\pass17-final3-sheets`

Validation proves exactly 28 PNGs, correct dimensions for every declared frame
count, all 11,592 frames represented once, and every sheet newer than its newest
source frame.

## Final visual gate

`PASS17-FINAL3-TRICK-REVIEW.md` reviews every current Trick tile plus exact
native/2x/4x plant, hold and recovery witnesses and returns PASS. Combined with
the exact 27-subject transfer above, this bank is **PASS 28/28** and is the only
bank authorized for encoding.
