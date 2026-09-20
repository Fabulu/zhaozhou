# Q009 tracearm-ordering
max_tokens: auto
root: C:\programmieren\zencrifice\zhaozhou-ceiling-lane-20260912

## Brief

`DebugTraceArm` (opcode 0xF003) is a new command that arms the console's trace
ring. `zhao_cmd_exec` walks a sealed command packet byte by byte and lowers each
record's effect; `zhao_cmd_decoder` walks the SAME byte stream in parallel (the
two are forked off one stream) and offers each record to `zhao_debug_trace` as
it goes. `zhao_debug_trace` stores an event only if the stage's bit is set in
`arm_mask_i`.

The author makes a GUARANTEE, in those words, and it is what you are checking:

> CMD.EXEC applies the mask at this record's LAST BYTE, during the walk.
> CMD.DECODER offers a record to the ring at the record's byte 15 (the end of
> its header), so:
>     the DebugTraceArm record itself is NOT traced;
>     every record after it in the same packet IS.
> That is a guarantee, not an approximation -- the next record's header cannot
> complete for at least sixteen more byte-cycles.

A second, separate claim: the arm is applied WITHOUT waiting for the packet's
verdict, unlike every other command this block executes, and a record with a
reserved bit set in `stage_mask` or `flags` is REFUSED WHOLE and counted in
`trace_arm_refused_o` rather than having the bad bits masked off.

A directed run measured `armed=0000001 stored=9 dropped=0 against decoder
records=11, arms=1, arm_refused=0` on an 11-record packet, and the author says
`stored = records - 2` because BeginFrame AND the DebugTraceArm record are both
offered to the ring before the arm lands. (The packet has since grown to 13
records and stored=11.)

You are shown the relevant slices of CMD.EXEC, the whole decoder, the whole
trace ring, and the C++ reference model the RTL is supposed to match.

## Questions

1. Is the guarantee TRUE as stated -- is there any record offset, record size,
   or backpressure pattern under which the DebugTraceArm record itself gets
   traced, or under which the record IMMEDIATELY after it does not? Pay
   attention to the smallest legal record and to what happens if the byte
   stream stalls between the arm's last byte and the next record's byte 15.
2. Is `stored = records - 2` a LAW or an accident of that packet's shape? State
   the general expression, in terms of the arming record's 1-based position.
3. Can `dbg_trace_arm_we_o` pulse more than once for one DebugTraceArm record,
   or for a record that is not a DebugTraceArm? Can it be lost entirely if the
   downstream is not ready on that exact cycle -- i.e. is it a pulse into
   something with no handshake?
4. Is the refusal really WHOLE? Name the exact bits checked, and say whether a
   refused record can still change `arm_mask_i` or `dbg_trace_clear_o` in any
   way. Also: is `trace_arm_refused_o` incremented exactly once per bad record?
5. Does the RTL agree with the C++ reference model field for field -- the same
   bits reserved, the same refusal, the same clear semantics? Name any
   disagreement.

Answer only about what is shown. If a question depends on code you were not
given, say so and name what you would need; do not guess.

## Inputs

fpga/rtl/command/zhao_cmd_exec.sv:480-560
fpga/rtl/command/zhao_cmd_exec.sv:600-640
fpga/rtl/command/zhao_cmd_exec.sv:900-960
fpga/rtl/command/zhao_cmd_exec.sv:1040-1130
fpga/rtl/command/zhao_cmd_exec.sv:1260-1290
fpga/rtl/command/zhao_cmd_exec.sv:1360-1420
fpga/rtl/command/zhao_cmd_decoder.sv
fpga/rtl/debug/zhao_debug_trace.sv
reference/include/zref/zref_trace.hpp
