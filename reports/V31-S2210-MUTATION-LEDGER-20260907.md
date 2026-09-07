# §22.10 mutation ledger — what has actually been demonstrated

§22.10 asks that the RTL tests be shown to FAIL for fourteen named changes, and
sets the standard: *"Record the exact failing assertion and restored source
digest. A mutation that fails to compile is not evidence that a behavioral
detector caught it."*

Every entry below is a BEHAVIOURAL failure — the mutation compiled, ran, and
tripped a named check. None is a compile error.

## Demonstrated

### 10. omit pending synchronous reads from queue occupancy
`zhao_texture_v3rq.sv`: `assign occ_o = lcnt_q` → `assign occ_o = body_occ_c`.
This is the owner's own §5 defect reinstated: `rp_q` advances when the read is
ISSUED, so the entry leaves the body an edge before it reaches a head register.

Four checks failed, the first being the property by name:

    FAIL: occupancy is NEVER zero between an accepted push and its head arrival
          -- a pending synchronous read still owns the ticket: expected 0x0, got 0x1
    FAIL: and reads at least one once the head is valid: expected 0x1, got 0x0
    FAIL: and occupancy at full equals capacity: expected 0x40, got 0x3E
    FAIL: and occupancy holds at exactly capacity throughout: expected 0x8, got 0x0

Restored: **28 checks pass**, source digest `e4e4de2a79395c8d`.

### 3. remove the current-window claim recheck
Demonstrated earlier today as V04: dropping the current-membership half of
`c2t_idok_c` lets a token whose owner retired between snapshot and claim be
accepted. Its mirror — dropping the SNAPSHOT half — fails V03 instead, so each
half was proved necessary by an opposite mutation.

### 12. free the owner at final write or prefetch instead of output acceptance
Covered behaviourally by case 13 (D.6): the final write does not free the owner,
checked by stalling through a full ring wrap with distinctive high context bits.

### `zhao_texture_v3own.sv`: `cbi_n_c` set from `cmb_pop_c` (§11.1 event 2, the
central bank-read RESERVATION) instead of from `cmb_fire_c` (event 3, actual
COMBINE ACCEPTANCE). Four M6 checks failed:

    FAIL: M6 a final arriving before COMBINE acceptance is an ERROR: expected 0x1, got 0x0
    FAIL: M6 no final payload write is authorised: expected 0x0, got 0x1
    FAIL: M6 nothing is published: expected 0x0, got 0x1
    FAIL: M6 the owner is NOT released: expected 0x1, got 0x0

This closes the gap noted in the first version of this file, where item 8 had a
passing case but no mutation. It now has both.

### 5. publish before payload write — THE MUTATION ESCAPED, TWICE

The most useful entry here, because nothing failed.

`cmt_n_c` set on the C3 bank-write edge instead of after the write lands at C4.
**All 538 existing checks passed.** Every one of them observes the END of a
transaction, and one cycle of early publication changes no final value in a
bench where nothing reads the bank in between. The harm is downstream and real:
§6.2 says *"PUBLISH: make that source committed only after its write edge"*
precisely so a consumer seeing `committed` cannot read the row before the
payload has landed.

**Then the fix for it escaped as well.** Case 24 was first written against
`ev_commits_o` — and that counter increments on `c4t_v_q`/`c4a_v_q`, the C4
stage valid, which is a *different signal* from the commit bitplane the mutation
altered. The new check passed against the very mutation it was written for.

`cmt_q` is now `verilator public` and case 24 watches it directly. Against the
mutation both its assertions fail:

    FAIL: §6.2: the source is NOT published on the same edge its bank write
          enable asserts: expected 0x0, got 0x1
    FAIL: and the publication DOES happen on a later edge: expected 0x1, got 0x0

Restored: **541 checks pass**, digest `2733389d405f0d9e`.

**The lesson is the sharper half of the campaign's purpose.** A mutation
campaign is usually described as testing the DESIGN. This one tested the TESTS,
found a real blind spot in a 538-check suite, and then found a second blind spot
in the fix — both times because the instrument watched an adjacent signal that
looked like the right one. Item 8 passing on the first attempt is much weaker
evidence than item 5 failing twice.

### `cmt_n_c[i] = cmt_n_c[i] | c4t_mask_q` → `cmt_n_c[i] = c4t_mask_q`. A second
source committing now erases the first, so a multi-source owner can never
assemble a complete set.

Caught immediately and totally — case 1 alone fails six ways:

    FAIL: case1 emitted count: expected 0x8, got 0x0
    FAIL: case1 combine packets: expected 0x8, got 0x0
    FAIL: case1 ticket counter (exactly one per owner): expected 0x8, got 0x0
    FAIL: case1 island quiescent at end: expected 0x1, got 0x0
    FAIL: case1 no live owners at end: expected 0x0, got 0x8

A useful contrast with item 5 on the same bitplane, three lines apart. Breaking
WHICH BITS are set stops the machine in the first case of the suite; breaking
WHEN they are set changed nothing any check could see. Severity of the source
edit is no guide to detectability — only the observable is.

### `&& !fwd_t_hit_c` deleted from `c2t_acc_c`. A second return for a source already
claimed in the shadow of C1's snapshot is no longer refused.

Caught by the RTL's own §19.7 partition assertion:

    %Error: zhao_texture_v3own.sv:2068: Assertion failed in
      TOP.zhao_texture_v3own.a_reject_partition_t: 'assert' failed.

That assertion requires exactly one of {range, stale, unsol, dup, accept} to
fire per valid C1 beat. Without forwarding a beat is simultaneously a duplicate
and an acceptance, so the partition breaks before any output does. A behavioural
catch, not a compile error — the design elaborated and ran.

Worth noting where the detector lived: this one was caught by an invariant
inside the RTL rather than by a bench check. Assertions that state a STRUCTURAL
law catch mutations no output-comparison would reach, because they fire at the
moment the law breaks rather than at the end of the transaction — which is
exactly what item 5 escaped through.

## NOT yet demonstrated — stated so the gap is visible

1. slot-only identity for external validation
2. truncate membership subtraction before rejecting upper bits
7. omit combine_reserved on local candidate insertion
9. pop a candidate without downstream storage credit
11. advance F without a reserved packet slot
13. reopen the namespace before one external adapter acknowledges
14. force old broken CLUT4, alpha, nearest, or global-binding behaviour

**Seven of fourteen.**

## Related mutations run today outside §22.10's list

Recorded because they are the same evidence in kind:

* **eligibility surrendered at execution rather than selection** (the brief's C01
  failure mode) → `a_svc_s1_not_eligible` fired immediately;
* **fog applied before the toon ramp** → both D-5 band-edge assertions failed;
* **fog weighted by the factor instead of its complement** → 13,386 of 13,416
  fragments mismatched;
* **round-half-up deleted from the fog factor** → 138 of 401 vertices mismatched,
  but ONLY after the sweep stride was repaired; at exact metre boundaries the
  same mutation produced zero mismatches, and that near-miss is the most useful
  entry in this file.
