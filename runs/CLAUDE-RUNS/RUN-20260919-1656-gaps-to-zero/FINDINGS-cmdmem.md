# FINDINGS — command/memory/measure lane (gz/cmdmem, final bea3e53e)

Transcribed by the coordinator. Commits: f4d08eca (R17 PublishResource, I47), 37bef328 (R20 + R32), 525a3f6d
(HPS arbiter pending slot, rule 6b; S1 fixed in MEM.UPLOAD and DEBUG.FRAMEBLIT), 3224bcef (R19 per-emitter
counter ids, 25 -> 72; append order restored; `counter_ids_append_only` guard), f8e003fa (R33 counts in the
language and compiler, FORM-E-611/612), bf792b13 (R18 MEASURE.TOKENS clamp composed; CMD.EXEC produces the
ceiling and requests), f5d56480 (R39 re-pins; the receipt fixture was left untouched because its other 16 hashes
were already stale), 362f8455 (format).

Arbiter: the lost pulse fixed (the new cases fail 6/128 on the old RTL). S1 was REAL in two clients (they spun on a
refusal while holding their request); their benches had modelled err only inside a granted burst, which the
real bridge never sends. S2 is not a defect (case 9 pins it). S3 is safe today; a one-beat write would end early
(no b_wr_ready_i).

Refused: MEASURE.GOVERNOR (no pixel_error/proj producer, no per-frame starvation latch, TERRAIN.LOD uncomposed);
I41 and I20's rest (R29 has no owner -> R53); I19/I45 readout (no HPS register path -> R51); I45 arming (-> R52);
I18 (no error-stream producer); the MEASURE.TOKENS request side (GEOM.BINNER's token client is tied off in the shell).

False claims: counters.md "append-only" was violated by 23 PREPENDING commits (`frame_cycles`, id 0 in RTL,
had become catalog index 150); packet_e pinned a formal property name that R32 split; the smoke's
closing note said MEM.UPLOAD was uncomposed.

Instrument defects: the ledger's rule stage never runs behind its failing schema stage, silencing V12/V15;
packet_h_sibling_diff failed on the stale shell hash before diffing, hiding 22 undeclared V2 changes;
`zhao_part_hps` ignores err while holding its request (-> R54).

Fast suite on its tree: 946/955; the 9 failures are in other lanes' files or pre-existing.