// oracle.ts — generator-internal frame-packet builder + validator, the golden
// PRODUCER (spec/capture_format.md 3). The C++ (zref_frame), consumer-TS
// (frame.ts) and SV (zhao_abi_pkg) validators are the TESTEES: they must agree
// with this code on every fuzz-corpus case, which is exactly what the
// conformance suite asserts. Sharing code between oracle and testees would
// weaken the test, so this file stands alone.

import { crc32c } from './crc32c.js';
import { LayoutIR } from './types.js';
import { DEBUG_OPCODE_HI, DEBUG_OPCODE_LO } from './layout.js';

export const FRAME_MAGIC = 0x314b505a; // 'Z','P','K','1' little-endian u32
export const HEADER_BYTES = 36; // sealed header incl. header_crc32c
export const FRAME_OVERHEAD = 40; // header + trailing payload_crc32c
export const OFF_MAGIC = 0;
export const OFF_ABI_VERSION = 4;
export const OFF_FLAGS = 6;
export const OFF_FRAME_ID = 8;
export const OFF_SEQUENCE = 12;
export const OFF_RESOURCE_EPOCH = 16;
export const OFF_DEADLINE = 20;
export const OFF_COMMAND_COUNT = 24;
export const OFF_COMMAND_BYTES = 28;
export const OFF_HEADER_CRC = 32;
export const HEADER_FLAG_CONTAINS_DEBUG = 0x0001;

export interface FrameHeaderInput {
  abiVersion: number;
  flags: number;
  frameId: number;
  sequence: number;
  resourceEpoch: number;
  deadline: number;
  commandCount: number;
  commandBytes: number;
}

export function buildFrame(
  ir: LayoutIR,
  hdr: FrameHeaderInput,
  commandStream: Uint8Array,
): Uint8Array {
  const out = new Uint8Array(FRAME_OVERHEAD + commandStream.length);
  const dv = new DataView(out.buffer);
  dv.setUint32(OFF_MAGIC, FRAME_MAGIC, true);
  dv.setUint16(OFF_ABI_VERSION, hdr.abiVersion, true);
  dv.setUint16(OFF_FLAGS, hdr.flags, true);
  dv.setUint32(OFF_FRAME_ID, hdr.frameId, true);
  dv.setUint32(OFF_SEQUENCE, hdr.sequence, true);
  dv.setUint32(OFF_RESOURCE_EPOCH, hdr.resourceEpoch, true);
  dv.setUint32(OFF_DEADLINE, hdr.deadline, true);
  dv.setUint32(OFF_COMMAND_COUNT, hdr.commandCount, true);
  dv.setUint32(OFF_COMMAND_BYTES, hdr.commandBytes, true);
  out.set(commandStream, HEADER_BYTES);
  dv.setUint32(OFF_HEADER_CRC, crc32c(0, out, 0, 32), true);
  dv.setUint32(HEADER_BYTES + commandStream.length,
    crc32c(0, commandStream), true);
  return out;
}

export interface ValidateResult {
  readonly error: number; // zhao_abi_error code (ZH_ABI_*)
  readonly commandsConsumed: number;
  /** bytes a decoder would have consumed: 36 on header-level abort, else 40+N */
  readonly bytesConsumed: number;
}

function get16(dv: DataView, off: number): number {
  return dv.getUint16(off, true);
}
function get32(dv: DataView, off: number): number {
  return dv.getUint32(off, true);
}

/**
 * Fail-safe validation, normative order (capture_format.md 3.2). Returns the
 * first error; every check runs before any payload field is consumed.
 */
export function validateFrame(ir: LayoutIR, pkt: Uint8Array, slotBytes: number): ValidateResult {
  const n = pkt.length;
  const ok = (error: number, commandsConsumed = 0, bytesConsumed = HEADER_BYTES): ValidateResult =>
    ({ error, commandsConsumed, bytesConsumed });

  if (n < 4) return ok(4); // BAD_LENGTH: not even a magic word

  const dv = new DataView(pkt.buffer, pkt.byteOffset, pkt.byteLength);

  // 1. magic (whenever the 4 bytes exist), then abi_version, reserved flag bits
  if (get32(dv, OFF_MAGIC) !== FRAME_MAGIC) return ok(1);
  if (n < HEADER_BYTES) return ok(4); // BAD_LENGTH: header incomplete
  if (get16(dv, OFF_ABI_VERSION) !== ir.abi.version) return ok(2);
  if ((get16(dv, OFF_FLAGS) & ~HEADER_FLAG_CONTAINS_DEBUG) !== 0) return ok(3);

  // 2. bounds
  const commandBytes = get32(dv, OFF_COMMAND_BYTES);
  const commandCount = get32(dv, OFF_COMMAND_COUNT);
  if (commandBytes % ir.abi.commandAlignment !== 0) return ok(4);
  if (FRAME_OVERHEAD + commandBytes > slotBytes) return ok(4);
  if (commandCount * 16 > commandBytes) return ok(4);
  if (n !== FRAME_OVERHEAD + commandBytes) return ok(4);

  // 3. header CRC (bytes [0,32))
  if (crc32c(0, pkt, 0, 32) !== get32(dv, OFF_HEADER_CRC)) return ok(5);

  // 4. payload CRC (command stream)
  if (crc32c(0, pkt, HEADER_BYTES, commandBytes) !== get32(dv, HEADER_BYTES + commandBytes)) {
    return ok(6, 0, FRAME_OVERHEAD + commandBytes);
  }

  // 5./6./9./10. record walk
  const sizeFor = new Map<number, number>();
  for (const c of ir.commands) sizeFor.set(c.opcode, c.recordBytes);

  let off = 0;
  let seen = 0;
  let anyDebug = false;
  while (off < commandBytes) {
    if (off + 16 > commandBytes) return ok(11, seen, FRAME_OVERHEAD + commandBytes); // TRUNCATED
    const opcode = get16(dv, HEADER_BYTES + off);
    const recordBytes = get16(dv, HEADER_BYTES + off + 2);
    if (recordBytes % ir.abi.commandAlignment !== 0 || recordBytes < 16) {
      return ok(4, seen, FRAME_OVERHEAD + commandBytes);
    }
    if (off + recordBytes > commandBytes) return ok(4, seen, FRAME_OVERHEAD + commandBytes);
    const want = sizeFor.get(opcode);
    if (want === undefined) return ok(7, seen, FRAME_OVERHEAD + commandBytes);
    if (recordBytes !== want) return ok(4, seen, FRAME_OVERHEAD + commandBytes);

    // 6. reserved fields: record header flags (no defined bits in v1), reserved0
    if (get32(dv, HEADER_BYTES + off + 8) !== 0) return ok(3, seen, FRAME_OVERHEAD + commandBytes);
    if (get32(dv, HEADER_BYTES + off + 12) !== 0) return ok(8, seen, FRAME_OVERHEAD + commandBytes);

    // 6b. payload pad bytes must be zero (zero-mask from the layout)
    const cmd = ir.commands.find((c) => c.opcode === opcode)!;
    for (const f of cmd.fields) {
      if (f.kind !== 'pad') continue;
      for (let b = 0; b < f.count; b++) {
        if (pkt[HEADER_BYTES + off + 16 + f.offset + b] !== 0) {
          return ok(8, seen, FRAME_OVERHEAD + commandBytes);
        }
      }
    }

    if (opcode >= DEBUG_OPCODE_LO && opcode <= DEBUG_OPCODE_HI) anyDebug = true;
    off += recordBytes;
    seen++;
  }

  if (off !== commandBytes) return ok(11, seen, FRAME_OVERHEAD + commandBytes);
  if (seen !== commandCount) return ok(13, seen, FRAME_OVERHEAD + commandBytes);
  if (anyDebug && (get16(dv, OFF_FLAGS) & HEADER_FLAG_CONTAINS_DEBUG) === 0) {
    return ok(12, seen, FRAME_OVERHEAD + commandBytes);
  }
  return { error: 0, commandsConsumed: seen, bytesConsumed: FRAME_OVERHEAD + commandBytes };
}
