# The bound comparison is done at 32 bits. The value was already 6.

Coordinator, 2026-09-28. Second instance of *a declared refusal that cannot
fire*, after `pw_t_illegal_w`. This one is sharper, because the guard is not
merely unwired — it is **wired to a value that has already lost the information
it was meant to test**, and it carries a comment explaining why it is careful.

Found by following a single lint warning:

```
%Warning-UNUSEDSIGNAL: zhao_field_host_v2.sv:1064:15:
    Bits of signal are not used: 'omap_src_c'[15:6]
```

That warning is not noise. It is the defect, stated precisely: *sixteen bits
were decoded and six were used.*

## The mechanism

The ABI declares the field as **u16** — `spec/form/field-host-image.md:351`:

> `| 4 | 2 | u16 | source_index | physical register for VECTOR_REG, prepared-scalar index for PREPARED_SCALAR |`

and declares the refusal — same file, `:127`, refusal case 2:

> **`VECTOR_REG` whose `source_index` is outside `[out_base, out_base + OUT_LANES)`**

The RTL decodes all sixteen bits, and then **stores six**:

```systemverilog
wire [15:0] omap_src_c = ld_data_i[15:0];                       // :1064
…
omap_index[ld_slot_i][ld_addr_i[ORDW-1:0]] <= omap_src_c[IDXW-1:0];   // :1561
```

`IDXW = max(REGW, PREPW) = max(5, 6) = 6`. The refusal then runs at header
time — correctly, and for a good reason the file documents at length — against
the **stored** value:

```systemverilog
if (omap_row_ok[…][wj] && (omap_kind[…][wj] == 1'b0) &&
    ((32'(omap_index[ld_slot_i][wj]) <  32'(ld_data_i[8 +: REGW])) ||
     (32'(omap_index[ld_slot_i][wj]) >= (32'(ld_data_i[8 +: REGW]) + 32'(OUT_LANES)))))
  winobs_viol_c = 1'b1;
```

So a descriptor naming `source_index = 68` is stored as `68 & 63 = 4`. With
`out_base = 4`, the window is `[4, 11)`, and **4 is inside it**. The refusal the
spec requires does not fire; the ordinal binds to register 4 and is reported as
satisfied.

## Why this survived

The check is preceded by this, at `:962`:

> `// EVERY BOUND COMPARISON HERE IS DONE AT 32 BITS, AND THAT IS NOT STYLE.`
> `// `IDXW'(REGS)` is the trap: IDXW is 6 and the shipped REGS is 64, so`
> `// `6'(64)` is ZERO and `x < 0` is constant FALSE for unsigned x …`

**That comment is correct, the widening it describes is correct, and it is why
nobody looked again.** The author found a real truncation trap in the
*comparison* and fixed it thoroughly. The truncation in the *operand* is three
hundred lines away, at the store, and the widened comparison cannot recover what
the store threw away. `32'(x)` on a 6-bit `x` yields a 6-bit value in a 32-bit
container.

This is CLAUDE.md's detector law almost verbatim — *"ask what the two sides of
the comparison are clocked by"* — with **width** in the place of timing. The
comparison can only catch faults in values the operand can still represent, and
an out-of-range index is precisely the fault it cannot represent.

It is also the broken-instrument law's direction: the defect makes the descriptor
look **valid**, so the refusal counter `bad_image_o` reads lower than the truth,
and nobody audits good news.

## Severity, stated honestly

**Not reachable on correct content**, exactly like `pw_t_illegal_w`. It needs a
descriptor whose `source_index` is ≥ 64 while the register file has 32 — an
image no correct producer emits, and `zref` does not emit one. The consequence
is a **silently aliased register read** reported as satisfied, not corruption of
unrelated state.

But the spec declares this refusal, the block implements refusals for every
*other* descriptor malformation by name (`hdr_countbad_c`, `hdr_formbad_c`,
`zero_mask_o`, `prep_bad_o`, `ld_oob_c`), and this is the one that is declared
and defeated. A refusal vocabulary with one silent hole is worse than none,
because the others establish the expectation that malformation is caught.

## The repair, and why it is not made here

One line, in principle: compare the **untruncated** `omap_src_c` against the
window, or store `omap_index` at a width that can hold what the ABI can express
and narrow only after the check. Which of those is right depends on whether the
stored width is load-bearing for area — `omap_index` is `[0:PROGS-1][0:OUT_ORDINALS-1]`,
so widening it is a real array-width change on a block already over budget, and
that is a measurement, not a guess.

**And it needs a test that can FAIL** — a descriptor with `source_index ≥ REGS`,
which no current bench produces, and an assertion that `bad_image_o` moves. That
is the same packet shape as the paramwalk one: build the malformed stimulus,
watch the counter move, then repair and watch it stay moved. Deliberately not a
tail-end edit to a refusal path.

## The argument this closes

209 generated `UNUSEDPARAM`s were drowning 27 real warnings in the board lint.
The case for clearing them was readability. **This is the return**: one of the 27
was a defect in a shipped refusal, and it was sitting in plain text — *"sixteen
bits decoded, six used"* — for as long as nobody could read the gate's output.

See also `FINDING-20260928-PARAMWALK-ILLEGAL-FLAG-HAS-NO-CONSUMER.md` (a refusal
flag with no consumer) and
`FINDING-20260928-STRIKING-A-SENTENCE-DOES-NOT-RETIRE-THE-CLAIM.md` (seven
assertions the tree refutes).
