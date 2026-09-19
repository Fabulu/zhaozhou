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

---

## THE UNWAIVED COUNT MOVED, 114 -> 120, and it is not a regression

2026-09-19, later the same day. The console closure grew from 125 files to 143
as four packets composed, and the GEOMETRY ASSET PATH brought
`zhao_geom_assetfetch.sv` in with it. That file carries SIX inherited
`-Wall` diagnostics that nobody touched and nobody introduced.

Attribution of the current 120, by file:

```
  86  zhao_texture_island_v3_top.sv
  17  zhao_render_texture_pkg.sv
   6  zhao_texture_binding_resolver_v2.sv
   6  zhao_geom_assetfetch.sv        <- NEW: joined the closure 2026-09-19
   3  zhao_texture_frag_expand_v2.sv
   1  zhao_console_core.sv
   1  zhao_texture_material_combine_v3.sv
```

**THE WAIVED RUN IS STILL SILENT, RC 0**, which is the half that actually gates.
The waiver matches `*fpga?rtl?geometry*`, so the six are covered; they show up
only in the raw run.

THE HAZARD THIS NUMBER ALWAYS HAD, now demonstrated: an absolute count is a
baseline that COMPOSING ANYTHING invalidates. A packet that adds a clean file
and a packet that adds a noisy one both "break" it, and the second is not worse
engineering -- it is a file the console now actually contains. Read the number
WITH its attribution or do not read it: "120, of which 6 are the asset path's
inherited diagnostics" is a fact; "120 > 114, therefore a regression" is not.

The gate that does not have this problem is the waived run, because it is an
absolute ZERO and cannot drift upward without something genuinely new and
unwaived appearing. Prefer it. Quote the unwaived number as evidence about
WHICH warnings exist, never as a pass/fail threshold.

A note on separators, because it has now cost two people time: the waiver globs
are `*fpga?rtl?texture*` with `?`, not `/`. They were forward-slash-only until
2026-09-19 and matched NOTHING when the caller passed Windows paths -- 112
phantom warnings in a subtree nobody had touched, which reads exactly like a
regression somebody just caused. `completion_register.closure_paths()` returns
backslash paths; convert with `.as_posix()` or rely on the `?` globs.