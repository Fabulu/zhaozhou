# Manafold Pass 17 exact final-bank integrity

**Date:** 2026-09-19
**Status:** **HISTORICAL INTEGRITY PASS / ART BLOCKED — batch 02 found the Death Drop cutoff; this bank must not ship**
**Tracked HEAD:** `c41816d84e14a398450910e26d071f001c2a685a`
**Accepted source commit:** `075d88af59873edd6e6a464d243a71ba84d0c094`
**Branch:** `manafold-pass17`
**Environment:** `ZIXX_EXP=celmain`, `ZIXX_LIGHT=diagonal-cool-cross`

This was an integrity receipt, not an art verdict. Batch 02 subsequently found Death Drop's f0233→f0234 soft-opaque effect cutoff. `PASS17-DEATH-DROP-EFFECT-REPAIR.md` records the focused repair; because production output changed, every hash and sheet below is historical blocker evidence and a new 28-subject bank is required. No encode, site update or deployment used this generation.

## Exact build receipt

A new caller-owned output was built from the tracked commits at
`.tmp/p17-final-bank-build`; no prior object or executable was reused.

- direct clean `cel` build: RC 0;
- renderer: `.tmp/p17-final-bank-build/bin/zhao-reel-cel.exe`;
- renderer MD5: `3390F2B8214473FEBFA32092A98E9056`;
- renderer SHA-256: `2A0C77BB321E09604AB5583147E54F2F4949D56C89EC25B8E0979DE0D4AF7E25`.

## Invocation correction

The first literal no-subject invocation revealed that this reel's default is the
entire global diagnostic catalogue, not the canonical Manafold bank. It produced
the required Manafold subjects plus unrelated Zixxtrixx, sky, terrain, lab and
probe subjects under `C:\programmieren\zencrifice\manafold-p16\pass17-final-reel`.
That mixed output is excluded from every acceptance and delivery manifest and was
left untouched.

The accepted render used the same hashed binary in one process with the exact 28
canonical subject names supplied together. This is the only final shipping root:

`C:\programmieren\zencrifice\manafold-p16\pass17-final-reel-28`

Renderer RC: **0**.

## Raw-bank validation

The committed `tools/reel/rgbframe.py` reader validated every frame header,
384×240 dimensions and exact byte count. Independent manifest validation also
required:

- exactly the canonical 28 subject directories, with no missing or extra subject;
- metadata `subject=` equal to the directory name;
- metadata `frames=` equal to the canonical clip length;
- exact zero-based contiguous `0000.rgb` filenames;
- exactly one contiguous `frame_NNNN_crc32c` receipt row per frame;
- all 11,592 ordered raw files included in the sequence SHA-256 values below.

**Result:** 28 subjects, **11,592 frames**, **3,205,048,896 bytes**, zero errors.

The bank-manifest SHA-256 is
`560c12371251dae73e39c7be6e6caad8e0817cfbc6ae15fce75e586697dfe30d`.
It hashes UTF-8 rows in sorted-subject order with tab-separated subject, frame
count, renderer sequence CRC32C, ordered-frame-byte SHA-256 and byte count.

| Subject | Frames | Sequence CRC32C | Ordered frame bytes SHA-256 |
|---|---:|---|---|
| manafold-blown | 292 | `0xFA396BEE` | `4369b638013e9de534eedaaae6ddafc7214ac7d91fcc9d7e41ba6b897e643900` |
| manafold-channel | 420 | `0xB26245C8` | `51801ec5ae51050f2f272670e33ed0483025dbd9037d61bdbebec057abe8fcf2` |
| manafold-crackle | 600 | `0x5232CEDF` | `c3b1296ce4424f84c8c3e70470882b37d45c35a85907b8cec461163ff0dfb0b2` |
| manafold-curious | 180 | `0xEA31677C` | `fd77bf220a1e89ce3988c6b0a35e2e141f3daaf633dbf03d42b74ee8e7a79947` |
| manafold-damage | 464 | `0xF6FFFAF1` | `74db58396b1f2e259bf88a926f1e29de81f37b16eb6efd27885bc2d4060ca83a` |
| manafold-death-drop | 450 | `0xFC405D06` | `28c2cf8668476505e23eb4e1a4f6646a1faae93bcc0e8f33097ec2ba74f9a534` |
| manafold-death-gutter | 590 | `0xF8C8CA5F` | `cbb9443255e218f428899a9befdf0a26c3bf20d1a445993a497977346c7f17a3` |
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
| manafold-trick | 400 | `0x58219E79` | `75c542516662902451ea01334fd7c8f51d9ea3acb68fd5eed767bcb4fea9aa0a` |

## Every-frame contact sheets

`tools/reel/plates.py` generated one sheet per subject with nearest-neighbour
4× downsampling (`scale=-4`) and 20 columns:

`C:\programmieren\zencrifice\manafold-p16\pass17-final-sheets`

Validation proves:

- exactly 28 PNGs, one per canonical subject;
- each sheet's dimensions encode `ceil(frame_count / 20)` rows;
- the generator received the complete sorted `*.rgb` set for each subject;
- every sheet is newer than its newest source frame;
- all 11,592 frames are represented exactly once across the 28 sheets.

## Disposition

Batch 01 passed 4/4. Batch 02 passed Damage, Death Gutter and Drift but correctly blocked Death Drop. Stop review of this generation. After the focused repair is committed and independently verified, clean-build and rerender all 28 canonical subjects into a new root, issue a new integrity manifest, and restart isolated review from those exact sheets.
