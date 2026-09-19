# Q001 memguard-r32-arm
max_tokens: auto
root: C:\programmieren\zencrifice\zhaozhou-ceiling-lane-20260912

## Brief

`zhao_mem_guard` is the memory-protection block: every VRAM write from every client passes
through it, and it refuses writes outside the regions that client may touch. Commit 37bef328
added owner ruling R32: the TERRAIN_BUILD client (which MEM.UPLOAD uses) gets a WRITE arm into
RENDER.ASSET_POOL. That arm is limited to the region named by `res_valid`/`res_base`/`res_span`,
and it opens only when that whole region lies inside RENDER.ASSET_POOL. Before R32,
RENDER.ASSET_POOL was read-only to every client. The formal property `mem_guard_no_escape` was
re-proved, and a committed mutant that drops the pool-containment check makes the proof fail.

You are shown the full current guard, the commit's diff of the guard, the reference model's
diff and the formal harness's diff. This is a SECURITY-style review: can anything get written
where it should not?

## Questions

1. Can the new arm let TERRAIN_BUILD write any byte outside RENDER.ASSET_POOL, or outside
   [res_base, res_base+res_span)? Look at overflow and wrap of base+span, span = 0, a region
   exactly at the pool's end, and a write burst that STARTS inside and ENDS outside.
2. Can the new arm let any client OTHER than TERRAIN_BUILD write into RENDER.ASSET_POOL?
3. Is the RTL's containment test exactly the reference model's (zref) test? Name any
   inequality whose strictness (< vs <=) or width differs.
4. Does the formal harness's change actually constrain the new arm, or could the proof pass
   vacuously (e.g. an assumption that makes res_valid never true, or an assertion that does
   not cover the new arm)?
5. If res_* changes WHILE a burst is in flight, can a burst that was checked against the old
   region write under the new one?

## Inputs

fpga/rtl/memory/zhao_mem_guard.sv
show:37bef328:fpga/rtl/memory/zhao_mem_guard.sv
show:37bef328:reference/include/zref/zref_mem.hpp
show:37bef328:tests/formal/formal_mem_guard.sv
