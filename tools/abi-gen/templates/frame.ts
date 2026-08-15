// frame.ts — consumer-side TS frame builder + sealed-packet validator.
// Hand-maintained mirror of reference/src/zref_frame.cpp and the SV
// zhao_frame_validate; law: spec/capture_format.md 3 (esp. the fail-safe
// validation order 3.2). Locked to the goldens and the tri-language parity
// suite (tests/fuzz, tests/differential), never to a shared implementation.

import {
  ZHAO_ABI_VERSION,
  ZHAO_COMMAND_ALIGNMENT,
  ZHAO_FRAME_FLAG_CONTAINS_DEBUG,
  ZHAO_FRAME_HEADER_BYTES,
  ZHAO_FRAME_MAGIC,
  ZHAO_FRAME_OVERHEAD,
  ZHAO_OFF_ABI_VERSION,
  ZHAO_OFF_COMMAND_BYTES,
  ZHAO_OFF_COMMAND_COUNT,
  ZHAO_OFF_FLAGS,
  ZHAO_OFF_HEADER_CRC,
  ZHAO_COMMAND_TABLE,
  ZH_ABI_BAD_ABI_VERSION,
  ZH_ABI_BAD_HEADER_CRC,
  ZH_ABI_BAD_VALUE,
  ZH_ABI_BAD_LENGTH,
  ZH_ABI_BAD_MAGIC,
  ZH_ABI_BAD_PAYLOAD_CRC,
  ZH_ABI_COUNT_MISMATCH,
  ZH_ABI_DEBUG_FLAG_REQUIRED,
  ZH_ABI_OK,
  ZH_ABI_RESERVED_FIELD,
  ZH_ABI_RESERVED_FLAG,
  ZH_ABI_TRUNCATED,
  ZH_ABI_UNKNOWN_OPCODE,
  ZHAO_OFF_MAGIC,
  crc32c,
} from './abi.js';

export interface ZhaoValidateResult {
  readonly error: number;
  readonly commandsConsumed: number;
  /** 36 on header-level abort, else 40 + command_bytes (capture_format.md 3.2) */
  readonly bytesConsumed: number;
}

function get16(dv: DataView, off: number): number {
  return dv.getUint16(off, true);
}

function get32(dv: DataView, off: number): number {
  return dv.getUint32(off, true);
}

/** Fail-safe validation, normative order (capture_format.md 3.2). */
export function validateFrame(
  pkt: Uint8Array,
  slotBytes: number,
): ZhaoValidateResult {
  const n = pkt.length;
  const fail = (error: number, seen = 0, consumed = ZHAO_FRAME_HEADER_BYTES): ZhaoValidateResult =>
    ({ error, commandsConsumed: seen, bytesConsumed: consumed });

  if (n < 4) return fail(ZH_ABI_BAD_LENGTH);
  const dv = new DataView(pkt.buffer, pkt.byteOffset, pkt.byteLength);

  // 1. magic (whenever the 4 bytes exist), header completeness, abi, flags
  if (get32(dv, ZHAO_OFF_MAGIC) !== ZHAO_FRAME_MAGIC) return fail(ZH_ABI_BAD_MAGIC);
  if (n < ZHAO_FRAME_HEADER_BYTES) return fail(ZH_ABI_BAD_LENGTH);
  if (get16(dv, ZHAO_OFF_ABI_VERSION) !== ZHAO_ABI_VERSION) return fail(ZH_ABI_BAD_ABI_VERSION);
  if ((get16(dv, ZHAO_OFF_FLAGS) & ~ZHAO_FRAME_FLAG_CONTAINS_DEBUG) !== 0) {
    return fail(ZH_ABI_RESERVED_FLAG);
  }

  // 2. bounds
  const commandBytes = get32(dv, ZHAO_OFF_COMMAND_BYTES);
  const commandCount = get32(dv, ZHAO_OFF_COMMAND_COUNT);
  const full = ZHAO_FRAME_OVERHEAD + commandBytes;
  if (commandBytes % ZHAO_COMMAND_ALIGNMENT !== 0) return fail(ZH_ABI_BAD_LENGTH);
  if (full > slotBytes) return fail(ZH_ABI_BAD_LENGTH);
  if (commandCount * 16 > commandBytes) return fail(ZH_ABI_BAD_LENGTH);
  if (n !== full) return fail(ZH_ABI_BAD_LENGTH);

  // 3. header CRC over [0,32)
  if (crc32c(0, pkt, 0, 32) !== get32(dv, ZHAO_OFF_HEADER_CRC)) {
    return fail(ZH_ABI_BAD_HEADER_CRC);
  }

  // 4. payload CRC over the command stream
  if (crc32c(0, pkt, ZHAO_FRAME_HEADER_BYTES, commandBytes) !==
      get32(dv, ZHAO_FRAME_HEADER_BYTES + commandBytes)) {
    return fail(ZH_ABI_BAD_PAYLOAD_CRC, 0, full);
  }

  // 5./6./9./10. record walk
  let off = 0;
  let seen = 0;
  let anyDebug = false;
  while (off < commandBytes) {
    if (off + 16 > commandBytes) return fail(ZH_ABI_TRUNCATED, seen, full);
    const opcode = get16(dv, ZHAO_FRAME_HEADER_BYTES + off);
    const recordBytes = get16(dv, ZHAO_FRAME_HEADER_BYTES + off + 2);
    if (recordBytes % ZHAO_COMMAND_ALIGNMENT !== 0 || recordBytes < 16) {
      return fail(ZH_ABI_BAD_LENGTH, seen, full);
    }
    if (off + recordBytes > commandBytes) return fail(ZH_ABI_BAD_LENGTH, seen, full);
    const info = ZHAO_COMMAND_TABLE.find((c) => c.opcode === opcode);
    if (!info) return fail(ZH_ABI_UNKNOWN_OPCODE, seen, full);
    if (recordBytes !== info.recordBytes) return fail(ZH_ABI_BAD_LENGTH, seen, full);

    // 6. reserved fields: record-header flags (no defined bits in v1), reserved0
    if (get32(dv, ZHAO_FRAME_HEADER_BYTES + off + 8) !== 0) {
      return fail(ZH_ABI_RESERVED_FLAG, seen, full);
    }
    if (get32(dv, ZHAO_FRAME_HEADER_BYTES + off + 12) !== 0) {
      return fail(ZH_ABI_RESERVED_FIELD, seen, full);
    }
    // 6b. declared payload pad bytes must be zero (padOffsets are payload-relative)
    for (let b = 16; b < recordBytes; b++) {
      if (info.padOffsets.includes(b - 16) && pkt[ZHAO_FRAME_HEADER_BYTES + off + b] !== 0) {
        return fail(ZH_ABI_RESERVED_FIELD, seen, full);
      }
    }
    // 7. enum fields must carry a declared member value (ABI v2)
    for (const ec of info.enumChecks) {
      let v = 0;
      for (let b = ec.size - 1; b >= 0; b--) {
        v = v * 256 + pkt[ZHAO_FRAME_HEADER_BYTES + off + 16 + ec.offset + b]!;
      }
      if (!ec.values.includes(v)) return fail(ZH_ABI_BAD_VALUE, seen, full);
    }

    if (opcode >= 0xf000 && opcode <= 0xf0ff) anyDebug = true;
    off += recordBytes;
    seen++;
  }
  if (off !== commandBytes) return fail(ZH_ABI_TRUNCATED, seen, full);
  if (seen !== commandCount) return fail(ZH_ABI_COUNT_MISMATCH, seen, full);
  if (anyDebug && (get16(dv, ZHAO_OFF_FLAGS) & ZHAO_FRAME_FLAG_CONTAINS_DEBUG) === 0) {
    return fail(ZH_ABI_DEBUG_FLAG_REQUIRED, seen, full);
  }
  return { error: ZH_ABI_OK, commandsConsumed: seen, bytesConsumed: full };
}

export interface ZhaoFrameHeaderInput {
  flags?: number;
  frameId: number;
  sequence: number;
  resourceEpoch: number;
  deadline?: number;
}

/**
 * Frame builder — the TS mirror of the C++ ZhaoFrameBuilder. Commands are
 * appended as serialized records (see the generated zhaoPack* functions);
 * seal() computes both CRCs per capture_format.md 3.
 */
export class ZhaoFrameBuilder {
  private readonly stream: number[] = [];
  private count = 0;

  /** append a pre-serialized command record (16-B header + payload) */
  appendRecord(record: Uint8Array | readonly number[]): this {
    for (const b of record) this.stream.push(b & 0xff);
    this.count++;
    return this;
  }

  seal(hdr: ZhaoFrameHeaderInput): Uint8Array {
    const commandBytes = this.stream.length;
    const out = new Uint8Array(ZHAO_FRAME_OVERHEAD + commandBytes);
    const dv = new DataView(out.buffer);
    dv.setUint32(ZHAO_OFF_MAGIC, ZHAO_FRAME_MAGIC, true);
    dv.setUint16(ZHAO_OFF_ABI_VERSION, ZHAO_ABI_VERSION, true);
    dv.setUint16(ZHAO_OFF_FLAGS, hdr.flags ?? 0, true);
    dv.setUint32(8, hdr.frameId, true);
    dv.setUint32(12, hdr.sequence, true);
    dv.setUint32(16, hdr.resourceEpoch, true);
    dv.setUint32(20, hdr.deadline ?? 0, true);
    dv.setUint32(ZHAO_OFF_COMMAND_COUNT, this.count, true);
    dv.setUint32(ZHAO_OFF_COMMAND_BYTES, commandBytes, true);
    out.set(this.stream, ZHAO_FRAME_HEADER_BYTES);
    dv.setUint32(ZHAO_OFF_HEADER_CRC, crc32c(0, out, 0, 32), true);
    dv.setUint32(ZHAO_FRAME_HEADER_BYTES + commandBytes,
      crc32c(0, out, ZHAO_FRAME_HEADER_BYTES, commandBytes), true);
    return out;
  }
}
