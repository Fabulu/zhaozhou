# Contract — CMD.DECODER (Command decoder)

> Ledger: `design/blocks.yml` · owner ZH-008 · phase 1 · maturity SPECIFIED

## Purpose and exclusions

Validate and dispatch semantic commands from sealed packets (generated ABI package); any malformed record becomes a safe error code, never a partial write.

Exclusions: no frame-slot ownership (CMD.SCHEDULER), no memory access. Packet layouts come EXCLUSIVELY from the generated ABI package (spec/commands.zidl → zhao_abi_pkg.sv); hand-written layouts are a review blocker. Malformed input: per-record length/opcode checks; a malformed record stops the packet with a safe error and no register/memory side effects (Dalvik model — reject before any write).

## Clock and reset semantics

Single `clk`, active-low async `rst_n` (negedge), `gpu` domain per the ledger —
the same domain CMD.DMA delivers bytes in, so there is no crossing here. Reset
clears the walk state, the running CRC, the record cursor, the error latch and
`commands_o`. No memory is reset (there is none).

## Input and output packet layouts

**Input is a BYTE STREAM, not a buffer.** That is the whole point of this block
and the reason `zhao_stub_top` is not it: the stub hands a whole slot to
`zhao_frame_validate` as a function call, which is a simulation convenience.
Silicon has no slot to hand over — CMD.DMA emits verified bytes one at a time
and this block must reach the same verdict having seen each byte once.

From CMD.DMA (already shipped, `zhao_cmd_dma.sv`):

| field | width | meaning |
|---|---|---|
| `pkt_valid_i` | 1 | a packet byte is offered |
| `pkt_ready_o` | 1 | this block accepts it |
| `pkt_byte_i` | 8 | the byte |
| `pkt_len_i` | 32 | verified total length, `40 + N` |

Out, ready/valid:

| field | width | meaning |
|---|---|---|
| `rec_valid_o` `rec_ready_i` | 1 | one decoded record header is presented |
| `rec_opcode_o` | 16 | the record's opcode |
| `rec_bytes_o` | 16 | `record_bytes`, a multiple of 16, at least 16 |
| `rec_source_id_o` | 32 | propagated per `spec/capture_format.md` §6 |
| `rec_index_o` | 32 | 0-based position in the stream |

Plus `decode_error_o` (the generated `zhao_abi_error` enum), `decode_done_o`,
`bytes_consumed_o` (32) and `commands_o` (32).

**The payload is NOT presented here.** This block decides record framing and
legality; consuming payload fields is the executor's job downstream. That keeps
the record port a fixed 96 bits regardless of which command it is.

## Backpressure rules

Backpressure: `ready_valid`.

## Memory ownership

**None, deliberately.** No buffer, no cache, no M10K, no VRAM port. The block
holds a 32-byte header shift register, a running CRC-32C, a record cursor and a
byte counter. That is the difference between this and a validator that needs the
packet in front of it, and it is why this block cannot repeat CMD.DMA's
1.97 Mbit mistake.

## Q formats and rounding

**None.** Every field here is an integer wire quantity — opcodes, byte counts,
CRC words, source ids. No fixed-point arithmetic exists in this block, so
`spec/qformats.md` does not apply to it. Stated rather than left blank so that a
future reader does not go looking for a rounding law that was never needed.

## Latency (fixed or variable)

Latency: `variable_bounded:4`.

## Target throughput

Target throughput: 1 record per clock.

## Overflow and malformed-input behaviour

**This section is the block.** `spec/capture_format.md` §3.2 gives ten ordered
checks, and the order is normative: every check runs BEFORE any payload field is
consumed, and on any error the frame aborts with no partial consumption.

| # | check | error |
|---|---|---|
| 1 | magic; length ≥ 36; abi_version; frame flags bits 1-15 zero | `BAD_MAGIC` / `BAD_LENGTH` / `BAD_ABI_VERSION` / `RESERVED_FLAG` |
| 2 | `40 + command_bytes ≤ FRAME_SLOT_BYTES`; `command_bytes % 16 == 0`; `command_count * 16 ≤ command_bytes`; length exactly `40 + command_bytes` | `BAD_LENGTH` |
| 3 | `header_crc32c` over bytes [0,32) | `BAD_HEADER_CRC` |
| 4 | `payload_crc32c` over bytes [36, 36+N) | `BAD_PAYLOAD_CRC` |
| 5 | per record: `record_bytes % 16 == 0 && ≥ 16`; running sum ≤ `command_bytes`; opcode known; `record_bytes == LayoutIR[opcode].size` | `BAD_LENGTH` / `UNKNOWN_OPCODE` |
| 6 | record `reserved0 == 0`; record `flags == 0`; payload pad bytes zero | `RESERVED_FIELD` / `RESERVED_FLAG` |
| 7 | enum ranges (v2: `video_mode` fields) | `BAD_VALUE` |
| 8 | handle generations vs `resource_epoch` | `STALE_HANDLE` |
| 9 | records sum exactly to `command_bytes`; walked == `command_count` | `TRUNCATED` / `COUNT_MISMATCH` |
| 10 | opcode in `0xF000-0xF0FF` requires frame flags bit0 | `DEBUG_FLAG_REQUIRED` |

**`bytes_consumed_o` is normative and easy to get wrong**: 36 on a header-level
abort (checks 1-3), otherwise `40 + N` — the whole packet is consumed before the
verdict even when a later check fails.

**The streaming difficulty, stated honestly.** Checks 3 and 4 are CRCs over
ranges that end before the data they protect has been fully seen, and check 9
cannot conclude until the last record. A streaming implementation therefore
cannot emit a record downstream the instant it parses one — it would be
publishing records from a packet whose payload CRC has not yet been checked, and
`ZH_ABI_BAD_PAYLOAD_CRC` must mean nothing was consumed. Two lawful shapes:

1. **Present records but gate their retirement** on `decode_done_o` with
   `decode_error_o == ZH_ABI_OK`, making the consumer responsible for discard.
2. **Decide first, replay after** — the packet is validated in one pass and
   records are emitted only afterwards, which requires the bytes to be available
   twice and therefore a buffer this block refuses to own.

**Shape (1) is CHOSEN.** It keeps this block memoryless, and the downstream
consumer is CMD.SCHEDULER, which already has frame-slot state and an abort path.
Shape (2) is rejected because it re-introduces exactly the whole-packet buffer
that made CMD.DMA unsynthesizable, for a block whose entire value is not needing
one.

## Counters and traces

Counters: `commands`. Source IDs: propagated.

## Scalar reference function

`zref::cmd::validate` in `reference/include/zref/zref_cmd.hpp`.

**A thin view onto an existing ratified law, not a second implementation.** The
ledger declared `zref::CmdDecoder` and that symbol never existed — the eighth
phantom `reference_model` found in this tree. But the LAW was already shipped:
`zhao::zhao_frame_validate` walks the ten ordered checks of
`spec/capture_format.md` §3.2 and is what `zhao_stub_top`, the capture tooling
and `tests/abi/golden/` already agree with.

Writing a fresh decoder oracle would have created exactly the drift this project
forbids — two implementations of one law, differing eventually, with tests
pinning whichever was consulted last. `zref_cmd.hpp` forwards and contains no
decode logic.

So "RTL matches the oracle" here means "RTL matches the function the stub shell,
the capture tools and 19 committed goldens have always agreed with", which is a
far stronger claim than matching something written alongside the RTL by the same
hand on the same day.

## Directed tests

Planned: `tests/command/cmd_decoder_directed.cpp`.

## Randomized differential tests

Planned: `tests/command/cmd_decoder_random.cpp`.

## Formal properties

None planned for this block.

## Synthesis / resource ceiling

Budget group: `command_debug` (§25). Per-block percentages unfrozen until Phase 0 (V5 gate).

## Integration capture cases

Composition with CMD.DMA is the point, and it is the seam where this block's
verdict has to agree with a producer that has already done its own validation.
`zhao_cmd_dma.sv` emits only bytes it has itself verified, so a disagreement
between the two is a real defect in one of them rather than a tolerance.

Not yet built. Named here so the composition is not skipped, following the
GEOM.BINNER precedent where composing with the real rasterizer immediately
exposed a tile-index-versus-pixel error that no isolated test could see.

## Notes

Contract filled (Phase-1-active). Byte layout comes from the generated zhao_abi_pkg.sv (W4) — never hand-written.
