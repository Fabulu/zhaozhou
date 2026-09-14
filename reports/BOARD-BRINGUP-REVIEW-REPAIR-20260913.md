# Board bring-up independent-review repair — 2026-09-13

**Reviewed snapshot:** `fe684755b8076b005214dc8dbbad7195b678b9f0`
**Completeness/provenance source fix:** `bd73428561da57d373c9da1ea449edbc444dc907`
**Board-truth binding verifier:** `bb441091`
**Actual-evidence verifier repair:** `6d0a2a8846f2eb250dc828b02d9931d3bfbbb060`
**Loader byte-identity repair:** `e66a606636727fb4df27aed87bd1181af01164eb`
**QSF-mutation build-entry repair:** `44a6d88576be382bf5eaa69ea0cf10fcc6d1982d`
**Projectless build-ID repair:** `2ee7d560d216170a791286b35a871573f8a8581a`
**State:** attempt 3 compiled/QSF-clean but failed artifact closure; **no compile or physical activity authorised**

## Review disposition

The historical observations remain credible:

- source/report/RBF/load hashes agree;
- custom FPGA loads and MENU rollback occurred;
- the owner-observed green display requires the 16 fixed comparisons and
  `e5f1c57f` signature in that image;
- post-map hierarchy contains one direct packed dual18 DSP;
- no JTAG, configuration flash, or persistent boot change occurred.

The historical build audits are nevertheless changed to
`historical-invalidated` and cannot authorise another load.

## Finding 1 — USER/SNAC was not actually released

Pinned upstream `fpga/sys/sys_top.v:1661-1667` maps USER_IO bits 2/4/5 to MiSTer
audio when physical `SW[1]` is asserted. `USER_OUT='1` in the core therefore did
not prove those pins high-impedance. The old fit report confirms permanently
disabled output enables only on bits 0/1/3/6.

### Repair

The upstream vendor tree stays byte-identical. Both board build scripts copy it
and run `tools/board/patch_mister_sys_top.py` against the workspace copy.

- upstream `sys_top.v` SHA-256:
  `9bc5562bcc9d923aa3bff1a9c976c52492edb9ef981f428b7a4101c919a711b8`;
- exact patched copy SHA-256:
  `24eea7b0f76848239c872f626a48f4e0c6150423b9e6561fd3dd63f2a99501e9`;
- all seven USER_IO assignments become unconditional `1'bZ`;
- all seven input observations read the physical USER_IO pins without SW audio
  substitution.

The transform requires the exact upstream digest and exactly one copy of each
replacement anchor. Mutation, missing-anchor, and double-patch controls fire.
Future fit verification requires disabled output-enable evidence for exactly
bits 0 through 6.

## Finding 2 — real Critical Warning was counted as zero

The historical map report contains:

```text
; mode ; Input ; Critical Warning ; Can't connect array with 4 elements ... to port with 5 elements ... ;
```

The old parser matched only `Critical Warning (<number>)`, so it returned zero.

### Repair

The build-copy overlay changes the scaler expression from four bits to five by
adding a leading zero. Existing meanings remain in bits 3:0. Verification scans
every report for any `Critical Warning` spelling, excluding only explicit zero
summaries. A committed tabular-warning fixture now fires in both Bringup and
Specs verifier tests.

Re-auditing the historical Specs workspace with the repaired verifier fails on:

- unpatched build-copy digest;
- missing unconditional USER_IO high-Z assignments;
- surviving SW[1] drive path;
- 4-bit scaler expression;
- the tabular Critical Warning;
- disabled output-enable set `[0,1,3,6]` rather than all seven.

## Finding 3 — loader identity and rollback claims were too weak

`StrictHostKeyChecking=yes` still trusted ambient host files, and state checks
looked for partial lines rather than requiring a complete exact record.

### Repair

`invoke_superstation_probe.ps1` now:

1. refuses identity/load evidence unless the loader file itself is clean at HEAD,
   then records its full source commit, exact Git blob, and working SHA-256;
2. obtains only the ED25519 key, calculates its SHA-256 itself, and requires
   `SHA256:FqNJOsj3FLUoMQxgn+cqGoXvVfENmVK4QFoSCMKl2lU`;
3. writes that one key to an ephemeral isolated known-hosts file and forces both
   user/global host-key lookup plus `HostKeyAlgorithms=ssh-ed25519` to it;
4. pins hostname, DT model/compatibility, Ethernet MAC, HPS silicon revision,
   MiSTer binary hash, and MENU RBF hash;
5. requires exact key sets and values for raw and parsed UTC, core, RBF identity,
   FPGA `operating`, all three named enabled bridges, and exactly one numeric
   MiSTer PID before/load/rollback;
6. requires physical loads to use HPS-watchdog-primary mode and a new explicit
   receipt path;
7. requires the exact contiguous watchdog fired/attempt/timeout/write-ok/MENU-ok
   sequence with matching summary attempts;
8. requires non-null/consistent build source, remote RBF, local RBF, V2 audit,
   complete manifest, audit/manifest/RBF source bindings, and rollback attempted/
   succeeded/staged-file-removed state;
9. requires new V2 build audit and complete manifest, so historical audits and
   RBFs cannot pass.

The first read-only identity attempt exposed Windows PowerShell 5's native-
stderr behavior around `ssh-keyscan`; explicit Process capture repaired it. The
next receipt passed identity but was stamped `fe684755`, before the loader source
binding existed, so it is preserved as `IDENTITY-PREFLIGHT-PRE-SOURCE-BINDING.json`
and is not cited as proof of the final loader. After committing the completeness
repair, a new read-only preflight bound itself to loader source commit
`bd73428561da57d373c9da1ea449edbc444dc907`, Git blob
`98fbb8f856e18cbb5faab01fee085399062fff00`, and working SHA-256
`692e735d0f8868723fa9c40a09770d1347cadf40b267ce8402354f37ce3b2d9b`;
the receipt validator independently resolves that commit:path blob and passes.

A separate read-only positive control aimed the repaired loader at the old Specs
RBF: pinned host and board identity passed, then the loader refused before SCP,
watchdog arm, or `load_core` because source no longer matched the old build.

## Complete manifest

`superstation_build_manifest.py` is integrated into both build scripts:

- pre-build source manifest: source commit, every repository build source,
  verification file, exact copied build input, and patched sys_top digest;
- complete manifest: immutable source-manifest digest plus the exact 16-file
  Quartus output set (flow/map/fit/asm/STA reports and summaries, smsg, done,
  JDI, pin, RBF, SLD, SOF);
- verifier independently reconstructs the exact profile source, verification,
  build-input and output record-name sets, rejects missing/extra names, checks
  every size+SHA-256, validates the canonical self-digest, and reconstructs and
  validates `sourceManifestSha256` from the complete record.

Source, artifact, self-digest, omitted-source-with-recomputed-self-digest,
added-output-with-recomputed-self-digest, and source-manifest-digest mutants all
fire. No V2 complete manifest exists yet because the review hold forbids the
repaired compile.

## Follow-up actual-evidence binding

Independent review found that the receipt verifier still accepted syntactically
valid but nonexistent build commits/evidence paths and an arbitrary 64-hex
manifest summary. Commit `6d0a2a8846f2eb250dc828b02d9931d3bfbbb060`
closes that verifier-only gap:

- both build scripts and both manifest profiles now bind the dot-sourced
  `tools/env/zhao-env.ps1` into the dirty/source closure;
- future-load validation requires a real repository and exact profile-specific
  V2 audit/manifest paths inside it;
- both evidence files must be clean, present at the receipt's loader-source
  commit, and byte-identical to those committed blobs;
- the build source commit must resolve, and the complete manifest verifier is run
  against the actual local build directory;
- the local RBF size and SHA-256 are recomputed, then cross-checked against the
  receipt, actual manifest, and actual audit;
- the actual manifest's canonical self-digest, exact record sets, source commit,
  source/verification diff against that commit, build marker, build inputs,
  patched `sys_top.v`, and all 16 artifacts are revalidated;
- the actual audit schema/profile/source, zero Critical Warnings, all seven
  disabled USER_IO output enables, and RBF record are cross-checked.

Committed controls fire for a nonexistent V2 file, nonexistent build commit,
arbitrary manifest digest, mutated local RBF, uncommitted audit mutation, source
commit drift, and a detached patched-`sys_top.v` record.

## Follow-up loader byte-identity binding

Independent audit of `57dc5ab0` found one remaining false-pass: repository-backed
identity validation checked the current loader working hash and the named
historical commit/blob separately, without proving they described the same
bytes. A fabricated receipt could therefore combine the real pre-binding
`fe684755` loader blob with the current loader's working SHA-256.

Commit `e66a606636727fb4df27aed87bd1181af01164eb` now reads the loader
blob bytes directly from `sourceCommit`, computes their SHA-256, and requires
both byte equality with the clean working loader and equality with the receipt's
`workingSha256`. The hostile control uses real commit
`fe684755b8076b005214dc8dbbad7195b678b9f0`, real differing loader blob
`32e22e0f527ebdd2245a002b8951dfee84b94262`, and the current working hash;
both the committed/working SHA check and direct byte comparison fire.

Format-only identity checks are explicitly quarantined as
`validate_identity_structure`. Production `validate_identity_receipt` now fails
closed without a repository, and a committed control proves that refusal.

## Verification performed without compile/load

- 64 repair/unit/mutation tests pass after the QSF-mutation runner repair.
- Both repaired source verifiers, board-truth validation, and source-bound
  identity-receipt validation pass; Python and PowerShell syntax checks pass.
- `board_truth.json` remains `partial` and explicitly holds future physical
  loads.
- Read-only identity preflight is source-bound to `bd734285` and its exact loader
  blob; independent receipt validation passes.
- Historical Specs build fails the repaired post-build verifier as intended.
- Historical RBF refusal receipt contains no loaded state, no remote path, no
  watchdog, and no rollback attempt.
- No repaired V2 build/audit/manifest existed before the compile-only gate below;
  physical loading remains HOLD.

## Compile-only gate activation — 2026-09-14

Independent re-review accepted exact head
`0ba2eefcd2938cd6a50a1853380ab4d8c9b8a629` and authorised exactly one
repaired V2 Specs compile through the committed manifest-bound build entry point.
The clean local and remote heads matched, no Quartus process was present, and a
new ignored build workspace was selected rather than replacing historical
artifacts. The build started at 04:53:46 UTC+02:00.

Acceptance requires the exact Cyclone V target/profile/source closure, successful
flow with zero errors and zero Critical Warnings, all seven USER_IO output
enables disabled, positive reported timing slacks, the expected physical
Zhaozhou hierarchy/DSP evidence, and matching audit/manifest/RBF hashes. Raw
reports, source manifest, failed audit, compile receipt and all raw outputs were
preserved. The complete manifest is intentionally absent because its creation
refused the build-input mutation.

### Compile outcome

Quartus itself completed successfully for `5CSEBA6U23I7` with 0 errors, 55
warnings and 0 Critical Warnings. The repaired verifier passed: all USER_IO bits
0–6 have disabled output enables; the expected Zhaozhou hierarchy and packed DSP
are present; all five reported internal timing slacks are positive. The measured
RBF is 2,448,816 bytes with SHA-256
`31699ff37440f26c8a979f53ce45b02ac63185038a2cc51a139eaba9ecb491eb`.

The overall entry nevertheless returned failure at the correct gate. Quartus
rewrote the copied `ZhaozhouSpecs.qsf` after source capture: the file changed
from 3,184 bytes / SHA-256
`0a4f036ec7aab3faf7a8b74a823add7485714107913b9156db690a1e9feebdd5` to
3,323 bytes / SHA-256
`c772638c5ac0ddf73739b1f4f47dbcaeb73a4c8218fce6a367556e2f1123841b`.
`LAST_QUARTUS_VERSION` changed edition and Quartus appended
`RESERVE_ALL_UNUSED_PINS_NO_OUTPUT_GND`. Complete-manifest creation refused the
hash/size mismatch. No complete candidate exists, the RBF is quarantined, and no
rerun occurred.

### Entry-point repair after failure

The build entries now call `run_superstation_quartus.py`, which invokes the real
Quartus compiler stages directly. `quartus_map`, `quartus_fit` and `quartus_asm`
receive `--read_settings_files=on --write_settings_files=off` in Quartus 17's
supported compiler-option position; `quartus_sta` follows without unsupported
flags. The helper snapshots QSF bytes and fails immediately if build-ID, map,
fit, assembly or timing changes them. Both helper and tests are included in each
profile's exact source/verification manifest closure.

Committed controls prove the flags reach the map/fit/assembly argument vectors,
the build entries no longer call `quartus_sh --flow`, a clean five-stage fixture
preserves QSF bytes, and the exact observed mutation shape—edition change plus
`RESERVE_ALL_UNUSED_PINS_NO_OUTPUT_GND` append—is rejected after map. The repair
is pushed as `44a6d88576be382bf5eaa69ea0cf10fcc6d1982d`; it has not been
compiled and requires independent review before any further Quartus action.

### Attempt 2 outcome

Independent Luna review accepted `8c91b784` for exactly one compile-only retry.
Preflight and source-manifest capture passed, but the guard found the pinned
upstream build-ID pre-flow itself mutates the QSF: `build_id.tcl` opens and closes
the project to read device/output assignments, and Quartus rewrote the same
edition/unused-pin fields before `quartus_map`. Build-ID returned RC 0; the QSF
guard returned 1; map, fit, assembly and timing were not run. There is no
attempt-2 RBF, resource/timing result or complete manifest. The attempt was
preserved under immutable `ATTEMPT2-BUILD-ID-QSF-MUTATION` names and stopped
without repeat or bypass.

### Projectless build-ID repair

Commit `2ee7d560d216170a791286b35a871573f8a8581a` keeps the pinned vendor
script unchanged and deterministically patches only the copied build input. It
requires upstream SHA-256
`148dc6a8124d36ac2898a45d085960cc05178a76d7297fde8877925fc0a74e88`,
produces projectless SHA-256
`e9a3daa3d507075abf214fefa115505a7669a14898800b728492f8a4393272c6`,
and removes `project_open`, `project_close` and `get_global_assignment`.
Revision, device `5CSEBA6U23I7` and `output_files` are now explicit source-bound
arguments to the exact production invocation.

The stage runner and complete manifest both require the exact patched build-ID
record and its copied build-input record. Controls prove pinned input/output,
mutated/missing/double-patch refusal, absence of project/settings access, exact
explicit production arguments, immediate rejection of the observed mutation,
and actual Quartus 17 execution of the projectless build-ID command without any
QSF byte change. The full focused suite passes 71/71; source, truth, bound
identity and syntax gates pass. No compile was run for this repair.

### Attempt 3 compile-only gate

Independent review accepted exact clean head `5666bce4` and authorised one
compile-only attempt through the projectless/stage-isolated runner. Clean
local/remote equality, a fresh attempt-3 workspace and no pre-existing Quartus
process were confirmed. Projectless build-ID and map/fit/asm/STA all returned RC
0; every QSF guard passed with the file fixed at 3,184 bytes / SHA-256
`0a4f036ec7aab3faf7a8b74a823add7485714107913b9156db690a1e9feebdd5`.
The repaired post-build verifier passed, with the same positive resource/timing
measurement and RBF identity as attempt 1.

The overall entry returned RC 1 at complete-manifest creation because the direct
stage sequence generated 15 of 16 exact outputs: `ZhaozhouSpecs.done`, normally
created by `quartus_sh --flow`, was absent. The exact artifact-set gate refused
rather than weakening its contract. No complete manifest exists; the RBF remains
quarantined. Attempt 3 is preserved under immutable `ATTEMPT3-MISSING-DONE`
names and no repeat/bypass occurred.

This authorisation did not include RBF staging/loading, SSH mutation,
watchdog/rollback rehearsal, JTAG, flash, persistence, pin drive, or board/SD
mutation. None occurred. Physical activity remains HOLD.

## Remaining gates

1. Commit/push immutable attempt-3 missing-marker evidence.
2. Obtain independent review before repairing completion-marker generation or
   running any further Quartus action; do not weaken the exact 16-artifact set.
3. Do not compile again unless a reviewed successor receives separate one-retry
   authorisation.
4. If a later retry passes, preserve a complete V2 candidate manifest and obtain
   explicit independent approval of those exact artifacts before physical action.
5. Physical red/failure control, green replay and nonce mailbox remain future,
   separately authorised gates.

JTAG, flash, persistent boot, SDRAM, external-I/O timing, broader arithmetic,
production migration, DSP saving, and full-shell claims remain open.
