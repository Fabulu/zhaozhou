# Board bring-up independent-review repair — 2026-09-13

**Reviewed snapshot:** `fe684755b8076b005214dc8dbbad7195b678b9f0`
**Completeness/provenance source fix:** `bd73428561da57d373c9da1ea449edbc444dc907`
**Board-truth binding verifier:** `bb441091`
**State:** repair-only; **no new Quartus compile or RBF load authorised**

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

## Verification performed without compile/load

- 45 repair/unit/mutation tests pass.
- Both repaired source verifiers pass.
- `board_truth.json` remains `partial` and explicitly holds future physical
  loads.
- Read-only identity preflight is source-bound to `bd734285` and its exact loader
  blob; independent receipt validation passes.
- Historical Specs build fails the repaired post-build verifier as intended.
- Historical RBF refusal receipt contains no loaded state, no remote path, no
  watchdog, and no rollback attempt.
- No repaired V2 build/audit/manifest exists; compile/load remain HOLD.

## Remaining gates

1. Commit/push the source-bound identity receipt and this completeness addendum,
   then submit exact head for independent review. Do not compile or load.
2. Confirm the separate 23:00 shell fit has completed before any later Quartus
   action.
3. Run one repaired clean Quartus build; create V2 audit and complete manifest.
4. Obtain explicit independent approval of those exact artifacts.
5. Run a separately named physical red/failure control, returned by the HPS
   watchdog.
6. Run a separately named green replay of fixed vectors.
7. Next architecture gate: HPS raw mailbox with nonce-selected vectors and
   independently host-readable raw results.

JTAG, flash, persistent boot, SDRAM, external-I/O timing, broader arithmetic,
production migration, DSP saving, and full-shell claims remain open.
