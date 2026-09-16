# G8B's four equivalence detectors, SEEN TO FIRE

*2026-09-16. Evidence about the instruments, not about the design.*

The G8B timing campaign added four simulation-only assertions, each guarding a
transformation whose correctness cannot be argued from the diff alone:

| assertion | file | guards |
|---|---|---|
| `a_win_mask_fresh` | `zhao_terrain_tess.sv` | T3a — the registered window mask equals the mask of the state actually in `ea`/`eb`/`j_level` |
| `a_cell_base_fresh` | `zhao_terrain_tess.sv` | T5 — the registered lattice base equals `cell_base(j_ox, ea, j_vshift)` |
| `a_blend_round_exact` | `zhao_terrain_tess.sv` | T7 — the pre-rounded rescale equals the untouched `rescale16` (**reverted; see below**) |
| `a_screen_fused_exact` | `zhao_project_core.sv` | T6 — the fused screen transform equals the two-function composition |

Every one of them had been **silent** across 110,592 jobs, which is exactly the
claim CLAUDE.md says to check hardest: *"a detector reading zero is a claim, and
it is the claim to check hardest."* Each guards a state legal stimulus cannot
reach — a paired assignment site that was missed, or a folded constant that is
wrong — so the only demonstration is to break it on purpose.

## The result

    FIRED: win_mask_q is stale        (pairing dropped at the advance site)
    FIRED: cell base is stale         (pairing dropped at the advance site)
    FIRED: fused screen               (shift 24 -> 23 in mad_to_screen)

Each mutation was applied to a backup-verified copy, built, run, and the file
restored in a `finally` block. `git status` is clean afterwards and the tests
pass from the restored tree: `terrain_tess_directed` 6,751, `proj_matw_directed`
242, `terrain_tess_modes_directed` 33.

`a_blend_round_exact` is not in the list because T7 was reverted the same day —
it measured 89.69 MHz against T5/T6's 97.61, because the register it folded a
constant into belonged to a DSP rather than the fabric. Its assertion went with
it.

## Three ways the FIRE TEST ITSELF lied first, all worth inheriting

### 1. It reported all three detectors dead, and the harness was broken

The first run printed **"DID NOT FIRE ... the detector is decorative"** three
times. The harness called

```powershell
$raw.Replace($Find, $Replace, 1)
```

and **PowerShell 5.1's `String` has no three-argument `Replace`**. The call
threw, the mutation never reached the file, the build was a no-op, the unmutated
test passed, and the harness concluded the detectors were decorative.

*Three detectors going dead simultaneously is far less likely than one harness
being broken.* That asymmetry is the only reason the conclusion was checked
instead of believed. The repaired harness asserts the pattern occurs **exactly
once**, that the mutated text **differs** from the original, and that it is
**non-empty**, before writing anything.

### 2. The same bug wrote a ZERO-BYTE source file

Debugging it by hand, the failed `Replace` left `$mut` as `$null`, and
`[IO.File]::WriteAllText($f, $null)` truncated `zhao_terrain_tess.sv` to **0
bytes**. It was restored from a backup taken one statement earlier.

This repository already has a chapter on this shape: a fire-test mutation once
coincided with a full disk and left a zero-byte backup beside deliberately
broken RTL. The mechanism was different and the outcome identical. **Write the
backup, read it back, and only then write the mutation** — which the harness now
does.

### 3. A FIRING detector looks exactly like a HUNG test

Every mutant run reached `$fatal`, printed its message, and then **sat at 0% CPU
forever**. That is the Verilated exit deadlock CLAUDE.md records, and it means

* the run has to be killed for the harness to proceed, and
* **"alive at zero CPU" here means the detector fired**, which is the opposite
  of what that signature means everywhere else in this repository.

The harness has no timeout; each hung mutant was released by hand. A timeout
belongs in it before it is used unattended.

## And the mutation has to be one the STIMULUS can see

`a_screen_fused_exact` did **not** fire on the obvious mutation — perturbing
T6's folded rounding constant `SCREEN_RND` by one. The detector was not at
fault. `(x + C + 1) >> 24` differs from `(x + C) >> 24` only where `x + C` is
congruent to −1 modulo 2²⁴: **exactly one input in 16,777,216**, and
`proj_matw_directed` drives 242 checks.

This is the second time the same fact bit on the same day. The independent
Python check of T6's algebra ran its negative control over ±2²⁰ — 2²¹ points,
a one-in-eight chance of containing the single disagreement — found none, and
printed **"THE CHECK IS BLIND."** It was a statement about the scan width, not
about the comparison.

**A negative control that samples too narrowly reports the instrument broken**,
and sends the next reader to rewrite a correct check. Changing the shift from 24
to 23 — which moves nearly every output — fired it immediately.

## What is still owed

These three are transient demonstrations, not committed evidence. The
repository's rule is that a guard unreachable by legal stimulus owes a
**committed mutant** under `tests/mutants/`, so the next person inherits the
evidence rather than the argument. That is owed here, and the reason it was not
done in this pass is that `zhao_terrain_tess.sv` is 1,600 lines and a committed
copy immediately falls under `mutant_copy_drift`'s maintenance — which is the
right cost to pay, but it is a packet of its own rather than a footnote to a
timing campaign.
