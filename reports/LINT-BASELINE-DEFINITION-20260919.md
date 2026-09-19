# The `zhao_console_core` lint baseline, stated unambiguously

Three different numbers have been quoted for "the lint result" this session and
all three were measured honestly. They differ because **the invocation differs**,
and every packet brief that says "≤114" or "0/0" needs to say which.

## Measured 2026-09-19 on the live tree

```
verilator --lint-only -Wall --top-module zhao_console_core <fit_targets closure>

  %Warning lines : 114
  %Error lines   : 1     <- "Exiting due to 114 warning(s)", the SUMMARY, not a fault
```

Composition of the 114:

```
  90  UNUSEDSIGNAL
  17  UNUSEDPARAM
   4  PINCONNECTEMPTY
   3  other
```

With `tests/shell/v3_closure_inherited.vlt` applied, the same closure is
**silent, RC 0**. That waiver file is the repository's existing, measured record
of diagnostics inherited from the V3 texture closure — it was not invented to
quieten this work.

## So the three quotes reconcile like this

| quote | invocation | correct? |
|---|---|---|
| "114 warnings, 0 errors" | `-Wall`, **no waiver** | **yes** |
| "0 diagnostics, RC 0" | `-Wall` **with** `v3_closure_inherited.vlt` | **yes** |
| "114 is the module count, the real total is zero" | — | **no** |

The third was offered as a correction to the first and is itself wrong: there is
no `in 114 modules` line in this run, and `%Warning` lines count exactly 114. It
was a reasonable inference from a *waived* run, and it would have retired a real
baseline — which is the dangerous direction, because a packet that then raised
the unwaived count from 114 to 130 would look like it had changed nothing.

**This is the third time today a number has been corrected in the flattering
direction and the correction has had to be checked as hard as the claim.** The
rule that keeps working: quote the command, not just the number.

## What a packet brief must say from now on

> `verilator --lint-only -Wall --top-module zhao_console_core` over the
> `design/fit_targets.yml` closure, **without the waiver**, must report **no more
> than 114 `%Warning` lines and no fault** (the `%Error: Exiting due to N
> warning(s)` summary is not a fault). Any rise must be justified in writing.
> Runs **with** `v3_closure_inherited.vlt` must be **silent**.

Both halves, because the waived run proves nothing new was added and the unwaived
run proves the inherited pile did not grow.

## The instrument trap found on the way, worth keeping

A worker ran the linter through `Start-Process` without the oss-cad-suite on
PATH. The process died with `0xC0000135` (missing DLL) and the wrapper reported
**"0 diagnostics"** — a clean bill from a program that never started. Caught by
positive-controlling the linter on a planted width mismatch first, which is the
only reason the zero was not believed. It is the launcher-popup trap wearing a
different exit code, and the defence is the same: fire the detector before
quoting its silence.
