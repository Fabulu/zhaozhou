# 43 fit rows were exempt from the structural gate — by a key mismatch, not by policy

2026-09-09. Found because I pre-registered the wrong status stamp for the
pair-pipe fit and went looking for why.

## The symptom, already documented as though it were a rule

`CLAUDE.md` and `tools/quartus/packet_accounting.py` both record it:

> `ruleViolations: []` on a **labelled** row is silence, not compliance: labelled
> rows are never rule-checked (0 of 26 carry violations, against 12 of 92
> unlabelled).

Counted in today's ledger, the ratio has only grown: **43 labelled rows carry 0
`ruleViolations`; 95 unlabelled rows carry 12.**

Both write-ups state the fact correctly and both read as if this were the design.
It is not. It is a lookup bug.

## The cause, in three lines

```
tools/quartus/fit_rules.ps1      $map[$top]              keyed "zhao_raster_perspuv_pairpipe"
tools/quartus/run_block_fit.ps1  $fitRules[$rowModule]   asks for "...@regfit"   -> $null
tools/quartus/fit_rules.ps1      Test-FitRules           if ($null -eq $Rules) { return $bad }
```

`$rowModule` is `"$mod$RowLabel"`. `Read-FitRules` keys its table on the bare
`- top:` name from `design/fit_targets.yml`. So **every labelled run asks for a key
that cannot exist**, gets `$null`, and `Test-FitRules` returns an empty list on a
null ruleset. The row's `status` therefore stays `ok` and no `ruleViolations` field
is ever written.

Demonstrated directly against the real rule table:

```
rule table: 40 keys; keys containing '@': 0

look up "zhao_raster_perspuv_pairpipe@regfit"  ->  ruleSet found: False   violations: 0
look up "zhao_raster_perspuv_pairpipe"         ->  ruleSet found: True
     VIOLATION: M10K 2 > allowed 1
     VIOLATION: registers 820 > allowed 700
```

Those two are exactly the violations I had to hand-check to write up the
pair-pipe result.

## Why this is the dangerous direction

It is the broken-instrument law precisely: **the defect makes rows look like
passes.** A labelled row that breaches every rule in its target still reports
`status: ok`, and `ok` is what a reader quotes. The gate whose entire purpose is
stated in the code —

> THE STRUCTURAL GATE. A block that met Fmax while putting its arrays in
> flip-flops used to report `ok`; now it reports the violation and the row is not
> a pass.

— has been inert for every labelled run since labels existed.

And it is self-concealing in the same way the `rd_gen_mismatch` counter was: the
absence of a `ruleViolations` field is indistinguishable from an empty one, so the
evidence of not-checking looks identical to the evidence of nothing-to-report.

## The fix, and why inheriting is right rather than merely convenient

One line, falling back to the unlabelled top:

```powershell
$ruleSet = $fitRules[$rowModule]
if ($null -eq $ruleSet) { $ruleSet = $fitRules[$mod] }
```

The objection would be that a labelled row is often a *parameter variant*, and a
variant might legitimately exceed the base budget. `design/fit_targets.yml`
answers that itself, on this very target:

> **SAME RULES AS svc DELIBERATELY.** The candidate exists to be smaller, so
> inheriting the budget it is trying to beat is the honest gate; a looser one
> would let it pass by being merely different.

So inheritance is the intended semantics, and a variant that breaches should be
stamped — that is the gate doing its job, not a false alarm. A variant that
genuinely warrants different limits should get its own `- top:` entry, which the
file already supports.

**Not applied yet.** `run_block_fit.ps1` is the script currently executing FIT
GATE 3's second half, and editing the file a running measurement was launched from
is a needless risk to a 39-minute job. The change and its test go in once that row
lands. The logic above is already validated in isolation against the real table.

## What it does NOT invalidate

**No measurement changes.** Every number in every labelled row is what Quartus
reported; only the pass/fail *stamp* was missing. The pair-pipe's 794 ALM, 820
registers and 119.25 MHz stand exactly as recorded.

**And the earlier caveats were right.** Both `CLAUDE.md` and `packet_accounting.py`
told readers not to trust a labelled row's silence. They were correct and they
prevented the wrong conclusion; what nobody did was ask *why* the silence existed.
A caveat that works is easy to leave in place forever.
