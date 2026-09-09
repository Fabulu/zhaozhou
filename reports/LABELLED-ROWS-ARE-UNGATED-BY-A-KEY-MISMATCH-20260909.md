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


---

# MEASURED: 18 labelled rows would breach, 11 of them are stamped `ok`

Applying the target rules to labelled rows by their base top -- i.e. what the
one-line fallback would do -- against the current ledger:

```
labelled rows: 43, of which have a rule set on their base top: 33
would breach:  18      of those, stamped `ok`: 11
```

The eleven stamped `ok` while breaching:

| row | breaches |
|---|---|
| `zhao_texture_island_v3_top@g2-prod` | ALM 10,837>7,500; regs 16,285>9,000; DSP 17>14 |
| `zhao_texture_island_v3_top@pktC` | ALM 15,911>7,500; regs 22,265>9,000; DSP 17>14 |
| `zhao_texture_island_v3_top@pktC-fixed` | ALM 15,483>7,500; regs 22,219>9,000; DSP 17>14 |
| `zhao_texture_island_top@p0b-island` | ALM 13,615>7,500; regs 23,295>9,000; DSP 17>14 |
| `zhao_texture_island_top@p0b-island-s3` | ALM 13,687>7,500; regs 23,267>9,000; DSP 17>14 |
| `zhao_texture_island_top@p0c-stageA` | ALM 11,562>7,500; regs 19,203>9,000; DSP 17>14 |
| `zhao_texture_v3own@v3-full` | ALM 5,678>1,800 |
| `zhao_geom_skin@MUL_LANES=6` | ALM 2,595>2,225; DSP 18>9 |
| `zhao_raster_rcp24_svc@g4-nctx12` | DSP 6>4; ALM 1,802>650; regs 1,514>600 |
| `zhao_raster_rcp24_svc@p0b-s1` | DSP 6>4; ALM 1,200>650; regs 1,037>600 |
| `zhao_raster_perspuv_pairpipe@regfit` | regs 820>700; M10K 2>1 |

## The demonstration that settles it: one module, both stamps

| row | label | breaches | status |
|---|---|---:|---|
| `zhao_texture_island_v3_top` | no | 3 | **`failed:structure`**, all three violations recorded |
| `zhao_texture_island_v3_top@g2-prod` | **yes** | 3 | **`ok`** |
| `zhao_texture_island_v3_top@pktC` | **yes** | 3 | **`ok`** |
| `zhao_texture_island_v3_top@pktC-fixed` | **yes** | 3 | **`ok`** |

Same module. Same three rules. Same three breaches. The unlabelled row is stamped
`failed:structure` and carries its violation list; the three labelled rows are
stamped `ok` and carry nothing. **`@pktC` breaches ALM by more than the unlabelled
row does — 15,911 against 13,133 — and reads `ok` while the smaller one reads
`failed:structure`.**

Nothing distinguishes them but the label.

## Why this matters beyond bookkeeping

`@g2-prod` is **the shipping profile**. It is the row this session has quoted all
day, and its `status: ok` is exactly what gets read as "the island passes its
gate". It does not: it misses three of its four resource rules, and separately
misses Fmax. The numbers were never wrong and the failure was known from scoring
against §21.6 by hand — but the ledger's own verdict field said the opposite of
the ledger's own numbers, and a reader who trusted the field would have been
misled about the one row that matters most.

## The second checker is blind too, by a different mechanism

`tools/quartus/check_fit_rules.ps1` does not share the key-mismatch bug. It
iterates `$rules.Keys` and finds the row where `module -eq $name`, so it only ever
examines the **unlabelled** row for each target and never visits a labelled one at
all.

So both checkers miss labelled rows, for two unrelated reasons, and neither by
policy. That is why the gap survived being noticed twice: each tool looked
individually reasonable.

Worth preserving from that file, since it is the same family of failure:

> A fit that failed, timed out, or was killed writes its row with every resource
> field null, and every rule in `Test-FitRules` is guarded by `$null -ne $x` — so
> all of them silently pass and the block is reported PASS in green.
> …`zhao_texture_tmu_pipe` reported PASS against a fresh `max_registers: 12000`
> while being the one block in the tree holding a 65,536-bit palette cache in
> flip-flops (72,824 registers). **The gate was green on the worst block it had.**
