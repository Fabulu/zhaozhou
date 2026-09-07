# P0-E — the island's top-level state, measured and attributed

The brief's P0-E: *"Top-level payload stores and metadata joins — 23,181 fitted
registers, duplicated state. One bank owner and credited joins; remove the old
side tables and reorder storage that the replacement makes redundant."*

This is that pool, measured rather than described.

## The pool exists and is the largest in the island

| | ALM | registers |
|---|---|---|
| sum of fitted member blocks | 11,211 | 16,314 |
| composed island (`@p0b-island`) | 13,615 | 23,295 |
| **top-level, above its members** | **+2,404** | **+6,981** |

`zhao_texture_island_top.sv` declares **twenty per-context side tables** of its
own, totalling **17,040 bits**:

    uvw_m[64] 64b   fctx_m[64] 64b   fbase_m[64] 32b   rob_m[64] 33b
    rob_tag_m[64] 16b   fwt_m[64] 8b   fpgn_m[64] 8b   flod_m[64] 8b
    fbind_m[64] 8b   fseq_m[64] 6b   frec_m[64] 3b   fsc_m[64] 2b
    fcls_m[64] 2b   fpsl_m[64] 2b   faux_m[64] 1b   rob_full_m[64] 1b
    sampmeta_m[16] 21b   palgen_m[16] 8b   class_m[16] 2b   palslot_m[16] 2b

## But 17,040 bits is NOT the prize, and assuming it was would have been wrong

The measured glue is ~6,981 registers against 17,040 declared bits, so most of
those tables are already cheap. The map report says which and why:

**Inferred as RAM** — `fctx_m`, `flod_m`, `fcls_m`, `faux_m` and others, sitting
in the island's 37 M10K rather than in logic.

**REFUSED, and Quartus names the reason:**

    Info (276007): RAM logic "uvw_m" is uninferred due to ASYNCHRONOUS READ
      LOGIC  ... zhao_texture_island_top.sv Line: 590
    Info (276007): RAM logic "class_m" is uninferred due to ASYNCHRONOUS READ
      LOGIC  ... zhao_texture_island_top.sv Line: 934

`uvw_m` is **64 bits × 64 entries = 4,096 flip-flops** — the single largest
uninferred array at the top level, and on its own a majority of the 6,981-register
glue pool.

## The actionable finding

P0-E's prize is not "delete 17,040 bits of duplicated state". It is **two arrays
whose READ IS ASYNCHRONOUS**, which is why they are flops instead of memory. The
same defect the L0 sweep found six times over, and the same one §16.2 found in
the DONE queue: *"a dynamically indexed combinational output from a flop array"*.

`uvw_m` alone is 4,096 registers, and registering its read would move it into an
M10K — the island has 553 available and uses 37.

**This also refines M3.** That entry says the v3own/fragrob integration's saving
lives in unmeasured deletions. Part of it is now measured and it is not a
deletion at all: it is a read-port change on two named arrays, independent of
the ownership rework and available without it.

## Not built

Diagnosis only, produced without a compiler while the island reseed runs. The
change touches `zhao_texture_island_top.sv`, which is inside that fit's closure,
so it waits regardless.
