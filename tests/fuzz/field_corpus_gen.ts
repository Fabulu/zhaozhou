// field_corpus_gen.ts — W5 Field IR fuzz corpus wiring note (plan W5
// deliverable 8).
//
// The corpus generator itself lives in the compiler workspace:
//   compiler/src/field_ir/fuzz_gen.ts (bounded-random earth programs built
//   through the typed builder — validity by construction)
// and runs as a committed-artifact test:
//   compiler/tests/field_fuzz_corpus.test.ts   (npm run -w compiler test)
// writing, on first run, and byte-comparing afterwards:
//   tests/fuzz/corpus/field/fuzz_seed_<seed>.zprog
//   tests/fuzz/corpus/field/fuzz_seed_<seed>.zvec
//
// Registration (documented choice): the corpus is COMMITTED at generation
// time (charter evidence discipline — generated files are evidence, not
// build artifacts), and the C++ side replays it as a CTest **nightly** test:
//   test_field_fuzz_parity  (tests/fuzz/test_field_fuzz_parity.cpp)
// so `ctest -L nightly` exercises the TS-vs-C++ interpretation differential
// over random programs; `fast` keeps only the deterministic crater_ring
// differential (plan R10 scope control). To regenerate: delete the corpus
// directory and run `npm run -w compiler test`, then commit.
