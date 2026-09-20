# Q011 register-reads-zero
max_tokens: auto
root: C:\programmieren\zencrifice\zhaozhou-ceiling-lane-20260912

## Brief

`completion_register.py` is the single number an entire engineering campaign is
being steered by. It parses a large SystemVerilog file's comment header — a
numbered list of "entries", each describing one function the console is
supposed to have and does not — plus a YAML capability ledger, and prints

    MANDATORY GAPS REMAINING : 27   (14 tie-offs + 11 disconnected + 2 unbuilt)

Work stops when that reaches ZERO, and a very expensive FPGA place-and-route
run is then started on the strength of it.

This codebase has a written law about exactly this situation:

> A broken instrument lies in ONE direction. The defect always made the answer
> look BETTER, SMALLER or SIMPLER than the truth. A parser that silently drops
> what it cannot match reports fewer problems. A self-check whose pattern
> matches nothing reports "no silent drops". **A number that is exactly zero is
> a broken instrument until proven otherwise** — precision at zero is a tell,
> not a result. Nobody audits good news.

YOUR JOB IS TO ATTACK THE TOOL, NOT THE DESIGN. Assume the design is full of
gaps and ask: what could this program fail to count? I am not asking whether
the 27 is right. I am asking what it would take for this program to print 0
while gaps remain.

It has a self-test that plants a gap and checks the parser classifies it. Treat
that self-test as a claim like any other.

## Questions

1. **Silent drops.** Find every place a regex, `if`, `continue`, `try/except`,
   `.get(...)` default or `startswith` can cause an entry, a capability or a
   line to be skipped WITHOUT being counted or reported. For each, say what
   input reaches it and whether the skip makes the total smaller.
2. **The zero path specifically.** Trace what has to be true for the printed
   total to be 0. Name every way the total can be 0 other than "there are no
   gaps" — an empty parse, a file that moved, a section header that changed
   spelling, an exception swallowed somewhere, a list that never got appended
   to.
3. **The self-test.** Does the planted gap exercise the SAME code path as a
   real entry, or a narrower one? Could the self-test pass while the real parse
   silently drops entries? Name what the self-test does NOT cover.
4. **Excuses.** Entries can be excused (deferred, superseded, waiting on a
   board). Can an entry be excused by a note that does not actually cite
   anything, or by a stale note whose claim is no longer true of the tree? Is
   the citation CHECKED or merely required to be non-empty?
5. **Asymmetry.** For each defect you find, state the DIRECTION of the error:
   does it make the count too high or too low? Rank your findings by "too low"
   first — an over-count is a nuisance, an under-count ends the campaign early.

Be concrete: file and line, the input that triggers it, and the direction.
Where the code is genuinely defensive, say so in one line and move on.

## Inputs

tools/budget/completion_register.py
