# FINDINGS R1 — TERRAIN.SHADE RTL

1. **The exact law costs 0 DSP / 1 M10K / ~700 ALM(struct) at II=147**,
   against a demand of one triangle per 833 clocks. The 10-DSP II=1
   estimate answered a rate nobody demands. MEASURED: 4,142/4,142 checks
   bit-exact vs the COMPILED `shade_flat_tri_dir_unclamped`; latency 145
   fixed; `--break-oracle` fails (instrument proven); all four counters
   exact (3,723/75/23/31).
2. **The contract's packet table carried the s1.15 error its own oracle
   history records** (sun AND base). The law is Q16.16; 53521 is odd and
   full-lit is 0x10000 — s1.15 cannot express either. Amended in place
   (A1/A2). The task brief inherited the same error ("in s1.15").
3. **Quarter-square M10K arithmetic is exact and shown**:
   `a*b = Q[a+b] - Q[|a-b|]`, checked exhaustively over 65,536 byte pairs
   in-suite; table rebuilt in 512 cycles at reset (no init file, M10K-safe).
4. **The isqrt is free in time**: |n|^2 completes 48 walk-cycles before
   ndot does; the 32-step root hides entirely under the sun products.
5. **Concurrent-lane note**: `check_prod_manifest.py`'s two residual
   UNACCOUNTED modules are the live forge lane's uncommitted files
   (RUN-20260909-2216); verified by differential stash and left alone; that lane registered them itself and the FINAL manifest check is OK (215 modules).
6. **Still owed before terrain is lit on screen**: the art-law LOOK gate
   (moving sun, 240p — ordered BEFORE RTL, not yet done), TERRAIN.PROJECT's
   colour port + fold seam, the sun ABI word, terrain in the composed
   shell, fit G-SHADE1 (batched).
