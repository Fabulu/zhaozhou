# Q010 answer — smoke-packet-arithmetic

- model: qwen38-quasar-dflash2-k8v4-112k  dispatch: a68b57aa272e4b26afac0dd6dc0662a4
- when: 2026-09-20T07:30:18  seconds: 444  finish: stop  status: ok
- usage: {"completion_tokens": 33322, "completion_tokens_details": {"reasoning_tokens": 29879}, "prompt_tokens": 9848, "prompt_tokens_details": {"cached_tokens": 0}, "total_tokens": 43170}
- inputs: [{"input": "tests/prod/tb_zhao_console_core_smoke.sv:3700-4000", "sha256": "64330fd7a1bd1f6a", "chars": 19627}, {"input": "tests/prod/tb_zhao_console_core_smoke.sv:1840-1870", "sha256": "fd2283ca71416d5d", "chars": 2281}]
- kind: task

## Answer

## FINDINGS
| # | P1/P2/P3 | file:line | claim | evidence |
|---|---|---|---|---|
| 1 | P2 | tests/prod/tb_zhao_console_core_smoke.sv:3991 | Build log/comment say 10 records, but the code copies 13 records and sets RECORD COUNT to 13. | Lines 3968/3991 say 10; 13 copy loops at 3938, 3942, 3944, 3946, 3949, 3951, 3953, 3955, 3957, 3959, 3961, 3963, 3965; RECORD COUNT `32'd13` at 3974. |
| 2 | P2 | tests/prod/tb_zhao_console_core_smoke.sv:3984 | No shown bound check that `o + ro + 4` fits `PKT_MAX_C` before the packet is written and `pkt_len_q` is set. | `PKT_MAX_C = 768` at 1856; no comparison to `PKT_MAX_C` in shown builder lines 3936-3984; writes at 3938-3983. |
| 3 | P3 | tests/prod/tb_zhao_console_core_smoke.sv:3971 | Header flags field is set to `0x0001` although the adjacent header comment says “flags 0”; the required value is not shown. | Comment at 3967 says “flags 0”; assignment at 3971 sets `16'h0001`. |

## ANSWERS

### 1. RECORD COUNT and BODY LENGTH
The code lays down **13** records:

- BeginFrame: copy 3938, `ro` += 32 at 3939  
- DebugTraceArm: copy 3942, `ro` += 32 at 3943  
- SetPresentationContract: copy 3944, `ro` += 48 at 3945  
- SetView: copy 3946-3947, `ro` += `ZHAO_SET_VIEW_BYTES` at 3948  
- PublishResource `pr`: copy 3949, `ro` += 48 at 3950  
- PublishResource `pr2`: copy 3951, `ro` += 48 at 3952  
- PublishResource `pr3`: copy 3953, `ro` += 48 at 3954  
- SetEnvironment: copy 3955, `ro` += 48 at 3956  
- SetPost: copy 3957, `ro` += 32 at 3958  
- SetGradeTable: copy 3959, `ro` += 96 at 3960  
- SetPopulation: copy 3961, `ro` += 48 at 3962  
- DrawForm: copy 3963, `ro` += 32 at 3964  
- EndFrame: copy 3965, `ro` += 32 at 3966  

RECORD COUNT is written as `32'd13` at tests/prod/tb_zhao_console_core_smoke.sv:3974, so it matches the 13 copied records.

BODY LENGTH is written as `32'(ro)` at tests/prod/tb_zhao_console_core_smoke.sv:3977, so it matches the final running offset.

If `ZHAO_SET_VIEW_BYTES` is the 112 bytes stated in the comments at tests/prod/tb_zhao_console_core_smoke.sv:1856 and tests/prod/tb_zhao_console_core_smoke.sv:3733, then `ro = 656`. The stale `records=10` display at tests/prod/tb_zhao_console_core_smoke.sv:3991 and comment at tests/prod/tb_zhao_console_core_smoke.sv:3968 are the mismatch, not the header field.

### 2. `ro` increments versus record/vector width
No mismatch is shown. Each `ro` increment matches the copied vector width:

- `bfv` declared 32 bytes at tests/prod/tb_zhao_console_core_smoke.sv:3730; copied 32 at 3938; `ro += 32` at 3939.
- `tav` declared 32 bytes at tests/prod/tb_zhao_console_core_smoke.sv:3751; copied 32 at 3942; `ro += 32` at 3943.
- `pcv` declared 48 bytes at tests/prod/tb_zhao_console_core_smoke.sv:3731; copied 48 at 3944; `ro += 48` at 3945.
- `svv` declared `8*ZHAO_SET_VIEW_BYTES` bits at tests/prod/tb_zhao_console_core_smoke.sv:3733; copied `ZHAO_SET_VIEW_BYTES` at 3946-3947; `ro += ZHAO_SET_VIEW_BYTES` at 3948.
- `prv`, `pr2v`, `pr3v` declared 48 bytes at tests/prod/tb_zhao_console_core_smoke.sv:3731; copied 48 at 3949, 3951, 3953; `ro += 48` at 3950, 3952, 3954.
- `sev` declared 48 bytes at tests/prod/tb_zhao_console_core_smoke.sv:3731; copied 48 at 3955; `ro += 48` at 3956.
- `spv` declared 32 bytes at tests/prod/tb_zhao_console_core_smoke.sv:3747; copied 32 at 3957; `ro += 32` at 3958.
- `gtv2` declared 96 bytes at tests/prod/tb_zhao_console_core_smoke.sv:3748; copied 96 at 3959; `ro += 96` at 3960.
- `spopv` declared 48 bytes at tests/prod/tb_zhao_console_core_smoke.sv:3731; copied 48 at 3961; `ro += 48` at 3962.
- `dfv` declared 32 bytes at tests/prod/tb_zhao_console_core_smoke.sv:3732; copied 32 at 3963; `ro += 32` at 3964.
- `efv` declared 32 bytes at tests/prod/tb_zhao_console_core_smoke.sv:3730; copied 32 at 3965; `ro += 32` at 3966.

### 3. Record zeroing / single fill
The record structs are cleared before any record fill:

- Clear line: tests/prod/tb_zhao_console_core_smoke.sv:3761 clears `bf`, `pc`, `sv`, `pr`, `ef`, `se`, `pr2`, `pr3`, `df`, `sp`, `gt`, `spop`, `ta`.
- Record fills begin after that at tests/prod/tb_zhao_console_core_smoke.sv:3764 onward.
- Pack calls read the filled records only after the fills, at tests/prod/tb_zhao_console_core_smoke.sv:3919-3932.

No shown record struct is cleared after being filled, read before being assigned, or duplicated by a rename.

One record-adjacent variable is assigned more than once in a way that is not a defect from the shown material: `gtv2` is assigned by the packer at tests/prod/tb_zhao_console_core_smoke.sv:3932 and then its vector bytes are overwritten at tests/prod/tb_zhao_console_core_smoke.sv:3933-3934. That is a post-pack fill of the grade-table vector, not a record struct cleared after being filled.

### 4. `PKT_MAX_C` size and bound check
`PKT_MAX_C` is 768 at tests/prod/tb_zhao_console_core_smoke.sv:1856.

With the documented 36-byte header and the comment-stated `ZHAO_SET_VIEW_BYTES = 112` at tests/prod/tb_zhao_console_core_smoke.sv:1856/3733, the final length is:

- `ro = 656`
- `o + ro + 4 = 36 + 656 + 4 = 696`

So `PKT_MAX_C = 768` is large enough for the shown packet.

There is no bound check in the shown builder. The writes at tests/prod/tb_zhao_console_core_smoke.sv:3938-3966, the header writes at 3969-3980, and `pkt_len_q = o + ro + 4` at 3984 do not compare the final length to `PKT_MAX_C`. If a later edit adds a record but leaves `PKT_MAX_C` unchanged, the code can write past the declared `pkt_mem` region and set `pkt_len_q` too large, with no shown assertion or check.

### 5. CRC coverage and placement
The trailing body CRC is correct in the shown code:

- Initial value at tests/prod/tb_zhao_console_core_smoke.sv:3981.
- Loop over `k < ro` at tests/prod/tb_zhao_console_core_smoke.sv:3982 covers exactly `pkt_mem[o + k]`, i.e. the body bytes from `o` through `o + ro - 1`.
- It does not include the header bytes before `o`, and it does not include the CRC’s own four bytes.
- It is written at `o + ro` through `o + ro + 3` at tests/prod/tb_zhao_console_core_smoke.sv:3983.
- The assignment `{pkt_mem[o+ro+3], ..., pkt_mem[o+ro]} = ~c;` puts the least-significant byte of `~c` at the lowest address, so the byte order is little-endian.

There is also a separate 4-byte CRC computed over `pkt_mem[0..31]` and written to `pkt_mem[32..35]` at tests/prod/tb_zhao_console_core_smoke.sv:3978-3980. That is not the trailing body CRC; whether that header CRC is required by the ABI is not shown.

### 6. Overlapping offsets / loops past fields
No overlapping `pkt_mem` record offsets are shown. Each record is copied at the current `o + ro`, and `ro` is then increased by the same byte count used for that copy, as listed in Q2. The running offset prevents one record from overwriting the next in `pkt_mem`.

The header writes use bytes 0..35 at tests/prod/tb_zhao_console_core_smoke.sv:3969-3980, and body writes start at `o`. If `ZHAO_FRAME_HEADER_BYTES` is the documented 36, there is no header/body overlap. The numeric value of `ZHAO_FRAME_HEADER_BYTES` is not shown in the provided code.

One unshown bound is the grade-table pre-copy loop at tests/prod/tb_zhao_console_core_smoke.sv:3933-3934: it writes 72 bytes into `gtv2` starting at `ZHAO_SET_GRADE_TABLE_OFF_VECTORS_0`. The value of `ZHAO_SET_GRADE_TABLE_OFF_VECTORS_0` is not shown, so it cannot be verified from the shown material whether `offset + 72` stays within the 96-byte `gtv2` vector declared at tests/prod/tb_zhao_console_core_smoke.sv:3748. The packet copy of `gtv2` itself is exactly 96 bytes at tests/prod/tb_zhao_console_core_smoke.sv:3959-3960.

## CONTINUATION
- Done: Q1-Q6 checked for the shown lines. Thirteen records are copied at tests/prod/tb_zhao_console_core_smoke.sv:3938, 3942, 3944, 3946, 3949, 3951, 3953, 3955, 3957, 3959, 3961, 3963, 3965; RECORD COUNT is 13 at 3974; BODY LENGTH is `ro` at 3977. All `ro` increments match vector widths at 3938-3966 against declarations at 3730-3751. Record structs are cleared at 3761 before fills and not cleared after fill; no record is read before its pack calls at 3919-3932. `PKT_MAX_C=768` at 1856 is enough if header=36 and SetView=112, but no bound check is shown around 3938-3984. Body CRC at 3981-3983 covers `pkt_mem[o..o+ro-1]` and writes little-endian at `o+ro`; a separate header CRC is at 3978-3980. No overlapping `pkt_mem` record offsets are shown.
- Open: Numeric package values are not shown in this chunk: `ZHAO_FRAME_HEADER_BYTES`, `ZHAO_SET_VIEW_BYTES`, `ZHAO_OFF_FLAGS`, and `ZHAO_SET_GRADE_TABLE_OFF_VECTORS_0`. Whether header flags=0x0001 at 3971 and the header CRC at 3978-3980 are required by the decoder is not shown. Code after 4000, including `dump_records`, is not shown.
- Next chunk: Verify the ABI package constants and decoder expectations for header flags/CRC and record count, and inspect `tests/prod/tb_zhao_console_core_smoke.sv:4000+` plus the CMD.DECODER/CMD.DMA code for bound checks. Needed inputs: `zhao_abi_pkg` definitions for header/record offsets and the decoder/HPS code that consumes `pkt_len_q`.

## Coordinator verdict

**verified** — All three findings real, all three fixed, and the arithmetic itself was CLEAN -- every ro increment matches its vector width, every record is cleared before it is filled and filled once, the body CRC covers exactly ro bytes at the right offset, and no two records overlap. That negative result is worth as much as the findings, because four separate merges have broken this builder. F1 is the best of them and it is the broken-instrument shape exactly: the header field said 13 while the BUILD LINE -- the evidence anybody actually reads -- printed a hardcoded records=10. Both now come from a counted 
rec, the same treatment o already had, so they cannot disagree. F2: there was no bound check at all against PKT_MAX_C; added, with a comment saying honestly that it names the cause rather than preventing the discarded writes. F3: the header comment said 'flags 0' while the code sets 0x0001; the code is right (bit 0 is ZH_ABI_DEBUG_FLAG_REQUIRED, mandatory for the 0xF003 record this packet now carries) and the comment was pre-R52. Severity sane this time.
