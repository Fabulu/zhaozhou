; divstep_formula.smt2 -- DSF-01 Appendix C, transcribed verbatim from
; reports/Zhaozhou_Divider_Fusion_Implementation_Guide.txt lines 1271-1285.
;
; Expected: unsat. The assertion is the NEGATION of equivalence, so unsat means
; no 63-bit dv and 31-bit d exist for which the two next-state functions differ.
(set-logic QF_BV)
(declare-fun dv () (_ BitVec 63))
(declare-fun d () (_ BitVec 31))
(define-fun t () (_ BitVec 32) ((_ extract 61 30) dv))
(define-fun tail () (_ BitVec 30) ((_ extract 29 0) dv))
(define-fun d32 () (_ BitVec 32) ((_ zero_extend 1) d))
(define-fun old_take () Bool (bvuge t d32))
(define-fun old_rem () (_ BitVec 32) (ite old_take (bvsub t d32) t))
(define-fun old_next () (_ BitVec 63) (concat old_rem (concat tail (ite old_take #b1 #b0))))
(define-fun diff () (_ BitVec 33) (bvsub ((_ zero_extend 1) t) ((_ zero_extend 2) d)))
(define-fun new_take () Bool (= ((_ extract 32 32) diff) #b0))
(define-fun new_rem () (_ BitVec 32) (ite new_take ((_ extract 31 0) diff) t))
(define-fun new_next () (_ BitVec 63) (concat new_rem (concat tail (ite new_take #b1 #b0))))
(assert (not (= old_next new_next)))
(check-sat)
