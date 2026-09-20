# Q010 smoke-packet-arithmetic
max_tokens: auto
root: C:\programmieren\zencrifice\zhaozhou-ceiling-lane-20260912

## Brief

This is the integration bench for a games console. It builds ONE sealed command
packet in `pkt_mem` and feeds it to the real command DMA, decoder and executor.
Four separate merges have broken this builder in four different ways, and every
automated gate stayed green each time, so it is reviewed rather than trusted.

The packet's shape: a 36-byte header, then N fixed-size records laid down at a
running offset `ro`, then a 4-byte CRC32C over the first `ro` bytes of the body.
Two header fields must agree with `ro`: a 32-bit RECORD COUNT at bytes 24..27
and a 32-bit BODY LENGTH at bytes 28..31. `PKT_MAX_C` sizes `pkt_mem` and must
be at least `o + ro + 4`.

Each record is built by a function that returns a fixed-width vector, and the
bytes are then copied into `pkt_mem` little-endian in a loop. Record sizes in
this packet are 32, 48, 96 and `ZHAO_SET_VIEW_BYTES` bytes.

Known history, so you can look for recurrences rather than re-deriving them:

* a record-CLEAR line (`bf = '0; pc = '0; ...`) was once moved BELOW two of the
  fills, so two records were zeroed after being filled and the whole packet was
  rejected with a symptom that appeared three subsystems away;
* two records were once given the same short variable name by a careless rename,
  so one silently overwrote the other;
* one record was written into a memory page that overlapped another, producing
  18 degenerate normals downstream;
* declarations were once placed after statements inside the task, which is
  illegal SystemVerilog.

## Questions

1. Does the header's RECORD COUNT equal the number of records actually laid
   down, and does the BODY LENGTH equal the final `ro`? Count the records in the
   code, do not trust the literal.
2. Does every `ro = ro + <n>` match the width of the record just written? Check
   each one against the vector's declared width, and name any mismatch.
3. Is every record variable ZEROED BEFORE it is filled, and is each one filled
   exactly once? Name any variable assigned twice, or read before assignment, or
   cleared after being filled.
4. Is `PKT_MAX_C` large enough for `o + ro + 4`, and is there any bound check?
   What happens if a later edit adds a record and forgets the literal?
5. Does the CRC cover exactly the bytes that were written -- not one byte fewer,
   not the header, not the CRC's own four bytes? And is it written at the right
   offset with the right byte order?
6. Are any two records written to OVERLAPPING offsets in `pkt_mem`, or does any
   record's copy loop run past its own field?

For each answer give the exact line numbers. If something is correct, say so in
one line; spend your words on what is wrong.

## Inputs

tests/prod/tb_zhao_console_core_smoke.sv:3700-4000
tests/prod/tb_zhao_console_core_smoke.sv:1840-1870