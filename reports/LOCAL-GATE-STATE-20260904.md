# The local fast lane is RED, and 18 of its tests do not run at all

**2026-09-04.** First full `ctest -L fast` of this session. The headline is not
the four failures; it is that **eighteen tests reported `Not Run`** — their
executables did not exist, so the lane reported a result for 472 of 490 tests
and said nothing about the rest.

That is the shape the memory *"local gates must match CI"* was written about:
a gate whose silence is indistinguishable from coverage.

## The four failures, and none of them is a false alarm

| test | cause | mine? |
|---|---|---|
| `format_check` | clang-format drift, **1,303 violations across ~40 files** | partly |
| `cppcheck_check` | `geom_project_directed.cpp:172` uninitialised `o.w` | **yes** |
| `golden_abi_info` | zidl / generator SHA-256 stale in golden captures | no |
| `zcap_roundtrip` | C++ writer output differs from the golden at equal length | no |

### `cppcheck_check` — the one worth the most

    tests/geometry/geom_project_directed.cpp:172:10: error:
      Uninitialized variable: o.w [uninitvar]

`out_w_o` was added to `GEOM.PROJECT` earlier the same day and **never
differentially verified**: `VtxOut` grew a `w` for the DUT to read into, the
oracle never set it, and `compare()` never checked it. The missing expectation
and the missing check cancelled, so **no test failure could reveal it**.

`zref::render::ProjOut` does not expose `clip.w` at all, so the fix is to make
it — additive, goldens must not move. Recorded in
`design/contracts/GEOM.PROJECT.md`.

**A static analyser found what 488 tests could not.** That is the argument for
keeping the lint tier green instead of reading it as noise beside the
differentials.

### `golden_abi_info` + `zcap_roundtrip` — one cause, not two

Both are downstream of the **QFMT_VERSION 2 -> 3 migration**
(`990de0d8`, `80f8666d`). `zcap_minimal.zcap` is 487 bytes and the writer
produces 487 bytes that differ — an embedded ABI_INFO hash, not a structural
change — and `golden_abi_info` reports the same stale zidl SHA directly.
`tables_tri` failing on *"tables.ts QFMT_VERSION marker missing"* is the third
face of it.

**This is an evidence question, not a mechanical one.** Regenerating goldens
makes the tests pass and destroys the only record of what changed, so it is not
done here. It wants whoever ran the migration.

### `format_check` — sanctioned remedy, deliberately separated

`.clang-format`'s own header says: *"the one-time mechanical reformat lives in
its own commit so blame/grief stays attributable."* The drift has returned at
1,303 violations. My four new files were part of it and are now clean; the
remaining ~40 files are a mechanical sweep that belongs in its own commit,
exactly as that comment prescribes.

## The 18 that did not run

    mem_upload_oracle        geom_assemble_directed   terrain_shade_oracle
    fragrob_differential     forge_prim_directed      geom_parambuf_directed
    twod_plane_directed      twod_plane_random        twod_sprite_directed
    twod_sprite_random       part_ladder_directed     part_ladder_random
    post_gather_directed     post_gather_random       geom_vdecode_directed
    part_record_directed     terrain_mipgen_directed  terrain_residency_v2_random

`ctest` does not build; it runs what exists. Every session that builds only the
targets it is working on leaves the rest stale or absent, and the lane then
passes on a subset without saying so.

**`fragrob_differential` is on that list**, and `fragrob` is in the current fit
queue — so the block about to be measured had no differential run against it
this session.

## What to do

1. **Build everything before trusting the lane.** In flight as this was written.
2. **Fix `ProjOut.w`** and give `out_w_o` a differential.
3. **Mechanical reformat**, its own commit, per `.clang-format`'s instruction.
4. **The QFMT_VERSION goldens want their author**, not a regeneration by
   whoever happens to notice.
5. **Consider failing the lane on `Not Run`.** A test that cannot run is not a
   test that passed, and today it was reported in the same breath as 472 that
   did.
