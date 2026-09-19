# Q003 post-echo-vs-zref
max_tokens: auto
root: C:\programmieren\zencrifice\zhaozhou-ceiling-lane-20260912

## Brief

POST.ECHO was just built: a contract first, then a C++ reference model (`zref::post::echo` in
`zref_post.hpp`), then the RTL `zhao_post_echo.sv`, checked by a directed test that compares the
RTL against the reference. The contract is the law. You are shown the contract, the RTL, the
reference header and the directed test.

This is a DIFFERENTIAL review. The question is not "is the RTL self-consistent", but "do the three
agree, and does the test actually prove it".

## Questions

1. Does the RTL implement the CONTRACT exactly? List every place where the RTL and the contract
   disagree (format, rounding, bit widths, ordering, edge behaviour, what happens at frame start and end).
2. Does the reference model implement the contract exactly? Same list. A place where the RTL and the
   reference AGREE with each other but DIFFER from the contract is the most important finding: the
   test cannot catch it.
3. Does the directed test compare the RTL against the reference over inputs that can reach every
   branch? Name any branch or edge case (e.g. saturation, the last pixel, a stall mid-frame, an
   arm/disarm between frames) that no test input reaches.
4. Is any counter or guard in the RTL wired so that it could never fire (both operands of a
   comparison loaded by the same enable, or a condition that cannot arise)?

## Inputs

design/contracts/POST.ECHO.md
fpga/rtl/compositor/zhao_post_echo.sv
reference/include/zref/zref_post.hpp
tests/compositor/post_echo_directed.cpp