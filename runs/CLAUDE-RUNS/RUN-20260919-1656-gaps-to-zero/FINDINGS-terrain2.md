# FINDINGS — terrain packet, pass 2

Transcribed by the coordinator from the packet's final report (the harness blocks subagent report files).

## Landed
* `fcfa8bbf` R16: TERRAIN.VISIBLE superseded by T5. The software side is the SW.STREAM REFERENCE
  (`zref_sw_stream.hpp` plus `sw_stream_directed.cpp`); no HPS runtime exists for any SW block. One ledger
  line to revert. The register's `superseded_by` is guarded: it needs a ruling, cited files that
  exist, and a built test. That guard was fired on 5 planted bad notes.
* `ce97827d` I28 CLOSED: the R14 contract `design/contracts/TERRAIN.WRITEBACK.DOORBELL.md`; the
  `zhao_terrain_jdoorbell` doorbell (342 checks, plus a committed overflow mutant); WRITEBACK composed.
  Its journal writes are client 2 of the 3-client HPS arbiter. The smoke cannot reach a writeback until
  BAKE dirties a page; the evidence is the terrain world test (167 checks), with the doorbell's real ticket.
* `786f52ba` R8: `zhao_terrain_loddev`, the first RTL for the LOD deviations. It has ONE selector,
  `DEV_INCLUDE_BOUNDARY`, locked to `zref::terrain::kLodDevIncludeBoundary` (the test fails if only
  one side changes; this was fired deliberately). 1,220 checks for both readings. Render:
  `reports/terrain-lod-readings/lod_readings_contact.png`. No gap closes until TERRAIN.LOD composes.

## Refused
* R15 BAKE / I32: BAKE's input is one circular dig per patch, while stamp results are per-texel
  strengths. That needs a per-vertex depth mode plus an A/B/C/D read and a B/D writeback path on the
  I26 socket. The 64→33 "shared edge" is ambiguous: a 64-texel sheet has no shared column.
* I26: waited for cmdmem's socket (now merged, e4164f11).
* R13: needs a layer-E reader plus a per-triangle material field all the way to the I13 merge.
* normalmap: waits on the I17 fragment stream. velocity: waits on the I34 field-uniform producers.

## Owner decisions (→ R21–R24)
1. R5 cannot be proved as ruled: 4,096 jobs × 456 clocks = 1.87M clocks, above the 1.67M-clock frame.
   SHADE is ~46× too slow. There is no sun-direction producer (SetEnvironment 0x0311 is only reserved).
2. R8: the readings are far apart. Morph gives 516 vs 1,891 and 904 vs 5,929 triangles at the two
   error scales tried, puts the coarsest level at the corner nearest the camera, and flattens the ridge,
   because level 3's comparison span (16 cells) is wider than a subpatch (8 cells).
   Recommendation: the mesh reading.
3. TERRAIN.ISLAND: its only instantiator was VISIBLE. Recommendation: supersede it by T5. R6 waits on this.
4. When the deviations are computed (page load vs per frame) is unruled.

## False claims
* I44 says the mip store owner is "UNIDENTIFIED". `spec/memory_rules.md` §5b defines TERRAIN.RESIDENT_MIP_POOL.
* The writeback header listed the guard read arm as a blocker; it landed on 2026-09-06.

## Instrument defects
* The register sliced its gap lists to 20 names, hiding `zhao_forge_cliff` and `zhao_post_gather`. Fixed.
* The slot-overflow mutant wrapper was 129 ports stale, because no gate runs the `-Mutant` smoke.
  Regenerated; it fires. **Add the `-Mutant` smoke to the gate list.**
* A signed-shift mistake in the packet's own RTL saturated every deviation; the differential test caught it.