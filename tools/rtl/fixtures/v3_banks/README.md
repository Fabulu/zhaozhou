# `check_v3_banks` fire-test fixtures

These are **deliberately wrong** SystemVerilog files. They are not part of any
build, are not in any fit source list, and must never be instantiated.

They exist because a detector that has not been shown to fire has not been
tested. `tools/rtl/check_v3_banks.py` runs every one of them on **every**
invocation and refuses to report on the real tree unless each fixture produced
the exact violation code it was written to produce.

| fixture | expects | the defect it plants |
|---|---|---|
| `good_bank.sv` | exit 0 | the Appendix B.6 shape, done right. The negative control. |
| `async_bank.sv` | `V3-ASYNC` | a declared bank read from a continuous `assign` — the "large asynchronous table" section 0 forbids |
| `two_writer_bank.sv` | `V3-MULTIWRITE` | two write addresses into one bank |
| `two_reader_bank.sv` | `V3-MULTIREAD` | two read addresses out of one bank |
| `logic_before_register.sv` | `V3-COMBLOGIC` | an adder between the array read and its first flop (QUARTUS_GOTCHAS §14) |
| `undersized_bank.sv` | `V3-WIDTH`, `V3-DEPTH` | 32×32 where section 6 declares 64×40 |
| `reset_payload.sv` | `V3-RESETPAYLOAD` | an async-reset payload process, against LAW 08 / §6.5 |
| `multidim_bank.sv` | `V3-MULTIDIM` | the D19m two-axis array shape that became 72,824 registers |
| `fabric_payload.sv` | `V3-FABRIC` | a 64×80 payload plane that is in no declared bank |
| `narrow_huge_payload.sv` | `V3-FABRIC` | the exact D19m shape: 16 bits wide, 4,096 rows, 65,536 bits — invisible to a width-only rule, and most of that block's 72,824 registers |
| `unresolvable.sv` | exit **2** | a width the tool cannot resolve — must be an error, never a pass |
| `no_banks.sv` | exit 0, and exit 1 under `--require-all` | a V3 file that declares no banks at all |
| `inst_good.sv` | exit 0 | a bank built the way the lane actually builds them — an instance of `zhao_texture_v3bank`. Negative control for the instance path. |
| `inst_wrong_geometry.sv` | `V3-WIDTH`, `V3-DEPTH` | `.WIDTH(32) .DEPTH(32)` where section 6 declares 64×40 |
| `inst_unbound.sv` | `V3-UNBOUND` | a bank primitive instantiated as anonymous scratch — a real M10K in no inventory. Run **twice**: fatal under `--require-all`, and printed as `[WARN]` with exit 0 without it. |
| `inst_no_geometry.sv` | exit **2** | an instance with no overrides and no reachable primitive defaults; the gate must refuse to guess |
| `inst_generate.sv` | exit 0, 3 banks | one generate loop that really is SAMPLE_RESULT_0/1/2, declared with a comma-separated marker. Negative control for the multi-bank form. |
| `array_multi_marker.sv` | exit **2** | the multi-bank marker abused on a single array — three inventory rows over one plane |

Two more detectors are fired from `check_v3_banks.py` itself rather than from a
file here, because what they watch is the tool's own configuration:

- `--require-all` on `no_banks.sv` must produce **13** `V3-MISSING` lines, one
  per declared bank (`_selftest_require_all`).
- `V3-TEMPLATE-GONE` must fire when a registered primitive is not in the tree,
  and must **not** fire for the real list (`_selftest_template_audit`). That
  audit has already earned its keep: it caught a `\b` that a shell heredoc had
  turned into a literal backspace byte, which made the module-declaration
  pattern match nothing.

To watch them all fire, with output:

```
python tools/rtl/check_v3_banks.py --fixtures
```
