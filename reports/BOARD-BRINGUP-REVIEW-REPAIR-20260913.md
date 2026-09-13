# Board bring-up independent-review repair — 2026-09-13

**Reviewed snapshot:** `fe684755b8076b005214dc8dbbad7195b678b9f0`
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

1. obtains only the ED25519 key, calculates its SHA-256 itself, and requires
   `SHA256:FqNJOsj3FLUoMQxgn+cqGoXvVfENmVK4QFoSCMKl2lU`;
2. writes that one key to an ephemeral isolated known-hosts file and forces both
   user/global host-key lookup plus `HostKeyAlgorithms=ssh-ed25519` to it;
3. pins hostname, DT model/compatibility, Ethernet MAC, HPS silicon revision,
   MiSTer binary hash, and MENU RBF hash;
4. requires exact key sets and values for UTC, core, RBF identity, FPGA
   `operating`, all three named enabled bridges, and exactly one numeric MiSTer
   PID before/load/rollback;
5. requires physical loads to use HPS-watchdog-primary mode and a new explicit
   receipt path;
6. requires complete watchdog fired/contiguous-attempt/write-ok/MENU-ok evidence;
7. requires new V2 build audit and complete manifest, so historical audits and
   RBFs cannot pass.

A read-only identity preflight passed and is preserved. A first attempt exposed
Windows PowerShell 5's native-stderr behavior around `ssh-keyscan`; it was
replaced with explicit `System.Diagnostics.Process` capture. A second read-only
positive control aimed the repaired loader at the old Specs RBF: pinned host and
board identity passed, then the loader refused before SCP, watchdog arm, or
`load_core` because source no longer matched the old build.

## Complete manifest

`superstation_build_manifest.py` is integrated into both build scripts:

- pre-build source manifest: source commit, every repository build source,
  verification file, exact copied build input, and patched sys_top digest;
- complete manifest: immutable source-manifest digest plus every file under
  Quartus `output_files`, including required flow/map/fit/asm/STA/pin/RBF/SOF;
- self-digest and source/build-input/artifact size+SHA-256 verification.

Source, artifact, and self-digest mutants all fire. No V2 complete manifest
exists yet because the review hold forbids the repaired compile.

## Verification performed without compile/load

- 36 repair/unit/mutation tests pass.
- Both repaired source verifiers pass.
- `board_truth.json` remains `partial` and explicitly holds future physical
  loads.
- Pinned read-only identity preflight passes.
- Historical Specs build fails the repaired post-build verifier as intended.
- Historical RBF refusal receipt contains no loaded state, no remote path, no
  watchdog, and no rollback attempt.

## Remaining gates

1. Commit and push this repair packet for independent review.
2. Wait for the separate 23:00 shell fit to complete.
3. Run one repaired clean Quartus build; create V2 audit and complete manifest.
4. Obtain explicit independent approval of those exact artifacts.
5. Run a separately named physical red/failure control, returned by the HPS
   watchdog.
6. Run a separately named green replay of fixed vectors.
7. Next architecture gate: HPS raw mailbox with nonce-selected vectors and
   independently host-readable raw results.

JTAG, flash, persistent boot, SDRAM, external-I/O timing, broader arithmetic,
production migration, DSP saving, and full-shell claims remain open.
