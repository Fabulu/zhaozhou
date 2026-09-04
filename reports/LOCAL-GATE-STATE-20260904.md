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

### THE ROOT CAUSE, MEASURED — and a stale binary was hiding it

**2026-09-04, later.** The three ABI failures are one cause, and a fourth test
joined them the moment a full rebuild happened. Byte-diffed the `z60` golden
against a freshly generated one:

    committed 3,476 bytes, regenerated 3,476 bytes
    68 bytes differ = 32 (generator SHA) + 32 (zidl SHA) + 4 (file CRC)

    runtime/include/zhao_abi.h  ZHAO_GENERATOR_SHA256  6B 95 FE 82 0B 7F ...
    captures/.../z60 @792                              DB 6F 6B 2B BF 7C ...

    runtime/include/zhao_abi.h  ZHAO_ZIDL_SHA256       A2 19 8A 6E 55 F7 ...
    captures/.../z60 @824                              E5 4F 05 D4 DD 1C ...

**The golden captures carry the ABI SHAs from before the regeneration.** Nothing
about the pictures, the counters or the frame packets differs — the two hash
fields and the file CRC that covers them, and nothing else. Same size, same
everything else.

### AND `shell_golden_replay` ONLY STARTED FAILING WHEN THE BINARIES WERE REBUILT

It passed in the 08:48 lane and failed in the 09:59 one. The tempting conclusion
was that something between them broke it — the `ProjOut` change sits exactly
there. **It did not.** `shell_golden.exe` had been built before the ABI
regeneration reached it, so it embedded the OLD constants and matched the old
golden. The full `cmake --build` at ~09:30 rebuilt it with the current
`zhao_abi.h`, and the drift became visible.

**A stale executable had been hiding a stale golden**, and the two agreed with
each other. That is `CLAUDE.md`'s stale-binary trap in its purest form: not a
wrong number, but a *right* number produced by a machine nobody had rebuilt.

**It also means the "18 Not Run" finding was worse than it looked.** Those 18
were visibly absent. This one was present, green, and wrong.

### THE FULL REBUILD EXPOSED A SECOND HIDDEN FAILURE, from the same migration

`fixp` — 1 failure in **29,385,065 checks**:

    test_fixp.cpp:816: gen::QFMT_VERSION == 2u (got 3 vs 2)

Commit `990de0d8` bumped `QFMT_VERSION` 2 -> 3 for amendment C2 (the particle128
v1 numeric law), and this pin still asserted 2. Like `shell_golden_replay`, it
had been **passing on a binary built before the bump reached it**.

**Fixed, and the justification is the amendment's own sentence** rather than a
shrug at a red test — `spec/qformats.md` C2 says *"No table or golden of §6/§7/§12
changed; the bump travels so capture replay can refuse pre-C2 [captures]"*, and
§6/§7/§12 are exactly the laws `test_fixp` covers. So C2 is a lane addition like
C1 before it, the pin moves to 3, and the comment says why.

**The pin is the point**: it does not track the version, it forces someone to
read the amendment and state why the laws still hold. Bumping it without that
sentence turns a gate into a formality.

### So the migration left FIVE marks, and stale binaries hid two of them

| test | mark |
|---|---|
| `golden_abi_info` | stale generator/zidl SHA in the captures |
| `zcap_roundtrip` | same SHAs, via the writer |
| `shell_golden_replay` | same SHAs — **hidden by a stale binary** |
| `fixp` | the `QFMT_VERSION` pin — **hidden by a stale binary** |
| `tables_tri` | `tables.ts` QFMT_VERSION marker (`qformats.md` §-bump requires "full regeneration + recommittance of tables") |

`fixp` is fixed here because it is a source pin with a written justification.
**The other four are the golden captures and the generated tables**, and those
still want whoever ran the migration: regenerating them turns four tests green
and destroys the only record of what changed.

### A claim of mine that was too fast

When `ProjOut` gained `w`, this session recorded *"render_golden PASS,
reel_sequence_crc PASS — the goldens not moving is the proof."* Those two did
pass and the proof holds for them. But `shell_golden_replay` was **in the same
run and failing**, and it was not noticed because only the pass list was read.

The conclusion (the `ProjOut` change is safe) survives — the diff above shows
the difference is two ABI hash fields that a struct field cannot touch. **The
method did not.** Reading a test run's passes without reading its failures is
not evidence, it is a preference.

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
