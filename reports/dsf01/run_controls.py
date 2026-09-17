r"""DSF-01 G1 failure controls: prove the SMT check can say `sat`.

The Appendix C formula returns `unsat`, which is the answer we want -- and an
`unsat` from a formula that cannot express disagreement is worth nothing. This
mutates the CANDIDATE side four ways, each a mistake the guide either names
explicitly or that a careful person could actually make, and requires every one
to come back `sat`.

Two of the four are the guide's own warnings:

  * BIT31   -- "Bit 31 can be one for a perfectly nonnegative 32-bit difference.
               It is not the borrow bit."
  * STRICT  -- "Equality is important: t=d produces ... quotient bit=1. A strict
               greater-than condition would be wrong."

and two are shapes the guide forbids in passing:

  * SIGNED  -- sign-extend instead of zero-extend ("the operands are not signed")
  * NOMUX   -- take the wrapped low bits unconditionally, i.e. a 32-bit
               subtraction "whose high bit is inspected after it has already
               wrapped"

A control that comes back `unsat` would mean the formula cannot see that class
of error, and the whole proof would be decoration.
"""
import io
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
BASE = os.path.join(HERE, 'divstep_formula.smt2')
Z3 = r'C:\programmieren\zencrifice\.tools\oss-cad-suite\bin\z3.exe'

base = io.open(BASE, encoding='utf-8').read()

NEW_TAKE = '(define-fun new_take () Bool (= ((_ extract 32 32) diff) #b0))'
NEW_REM = ('(define-fun new_rem () (_ BitVec 32) '
           '(ite new_take ((_ extract 31 0) diff) t))')
DIFF = ('(define-fun diff () (_ BitVec 33) '
        '(bvsub ((_ zero_extend 1) t) ((_ zero_extend 2) d)))')

CONTROLS = {
    'BIT31': (
        NEW_TAKE,
        '(define-fun new_take () Bool (= ((_ extract 31 31) diff) #b0))',
        "reads bit 31 as the borrow; the guide's named trap"),
    'STRICT': (
        NEW_TAKE,
        '(define-fun new_take () Bool (bvugt t d32))',
        'strict > instead of >=; wrong exactly at t == d'),
    'SIGNED': (
        DIFF,
        ('(define-fun diff () (_ BitVec 33) '
         '(bvsub ((_ sign_extend 1) t) ((_ sign_extend 2) d)))'),
        'sign-extends operands that are unsigned'),
    'NOMUX': (
        NEW_REM,
        '(define-fun new_rem () (_ BitVec 32) ((_ extract 31 0) diff))',
        'takes the wrapped difference unconditionally'),
}


def z3_run(text, path):
    io.open(path, 'w', encoding='utf-8').write(text)
    p = subprocess.run([Z3, '-smt2', path], capture_output=True, text=True)
    return (p.stdout or p.stderr).strip().splitlines()[-1].strip()


def main():
    rc = 0
    base_path = os.path.join(HERE, '_control_base.smt2')
    verdict = z3_run(base, base_path)
    print('PRODUCTION       %-6s (want unsat)' % verdict)
    if verdict != 'unsat':
        print('  the production formula does not prove equivalence')
        rc = 1

    for name, (old, new, why) in CONTROLS.items():
        assert base.count(old) == 1, 'control %s: anchor not unique' % name
        text = base.replace(old, new, 1)
        assert text != base
        path = os.path.join(HERE, '_control_%s.smt2' % name.lower())
        v = z3_run(text, path)
        ok = (v == 'sat')
        print('CONTROL %-8s %-6s (want sat)  %s  -- %s'
              % (name, v, 'FIRED' if ok else 'MISSED', why))
        if not ok:
            rc = 1
        os.remove(path)
    os.remove(base_path)

    print()
    print('G1 arithmetic evidence: %s' % ('PASS' if rc == 0 else 'FAILED'))
    return rc


if __name__ == '__main__':
    sys.exit(main())
