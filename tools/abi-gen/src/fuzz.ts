// fuzz.ts — deterministic malformed/valid frame-packet corpus (P5 6.5,
// capture_format.md 6). Cases are built from the golden sample records; the
// ORACLE validator computes each expected error at generation time, and every
// testee (C++ zref_frame, consumer-TS frame.ts, SV zhao_abi_pkg via the
// Verilated probe) must reproduce exactly those codes.

import { crc32c } from './crc32c.js';
import { LayoutIR } from './types.js';
import { sampleCommand, sampleRecordBytes } from './sample.js';
import {
  FRAME_MAGIC, FRAME_OVERHEAD, HEADER_BYTES, OFF_ABI_VERSION, OFF_COMMAND_BYTES,
  OFF_COMMAND_COUNT, OFF_FLAGS, OFF_HEADER_CRC, buildFrame, validateFrame,
} from './oracle.js';

export interface CorpusCase {
  readonly name: string;
  readonly packet: Uint8Array;
  readonly expectedError: number;
}

function reframe(pkt: Uint8Array): Uint8Array {
  // re-seal both CRCs over the (possibly mutated) header+stream
  const dv = new DataView(pkt.buffer, pkt.byteOffset, pkt.byteLength);
  const commandBytes = dv.getUint32(OFF_COMMAND_BYTES, true);
  const out = new Uint8Array(pkt);
  const odv = new DataView(out.buffer);
  odv.setUint32(OFF_HEADER_CRC, crc32c(0, out, 0, 32), true);
  odv.setUint32(HEADER_BYTES + commandBytes, crc32c(0, out, HEADER_BYTES, commandBytes), true);
  return out;
}

function mutate(pkt: Uint8Array, off: number, byte: number): Uint8Array {
  const out = new Uint8Array(pkt);
  out[off] = byte;
  return out;
}

function mutate16(pkt: Uint8Array, off: number, value: number, resealed: boolean): Uint8Array {
  const out = new Uint8Array(pkt);
  const dv = new DataView(out.buffer);
  dv.setUint16(off, value, true);
  return resealed ? reframe(out) : out;
}

function mutate32(pkt: Uint8Array, off: number, value: number, resealed: boolean): Uint8Array {
  const out = new Uint8Array(pkt);
  const dv = new DataView(out.buffer);
  dv.setUint32(off, value, true);
  return resealed ? reframe(out) : out;
}

/** Build the corpus. Deterministic: same .zidl => same bytes, same codes. */
export function buildCorpus(ir: LayoutIR, slotBytes: number): readonly CorpusCase[] {
  const byName = new Map(ir.commands.map((c) => [c.name, c] as const));
  const cmd = (name: string): Uint8Array => {
    const c = byName.get(name)!;
    return sampleRecordBytes(c, sampleCommand(ir, c));
  };

  const base = buildFrame(ir, {
    abiVersion: ir.abi.version,
    flags: 0,
    frameId: 1,
    sequence: 0,
    resourceEpoch: 1,
    deadline: 0,
    commandCount: 3,
    commandBytes: 80,
  }, new Uint8Array([...cmd('BeginFrame'), ...cmd('Nop'), ...cmd('EndFrame')]));

  // COMMAND BYTES ARE MEASURED FROM THE PAYLOAD, NOT RESTATED.
  // This read `32 + 96 + 48 + 16 + 32`, and the 96 was SetView's size before
  // owner ruling R63 grew it to 112 (`fx16 eye[3]` + `pad[4]`). The payload
  // then measured 240 while the header still declared 224, so the frame the
  // corpus calls VALID validated to ZH_ABI_BAD_LENGTH -- the corpus was
  // testing the validator with a deliberately malformed frame and calling the
  // result a regression. It was the THIRD hardcoded copy of that one record
  // size, after capture_format.md 1.3 and abi_gen.test.ts.
  const fullPayload = new Uint8Array([
    ...cmd('BeginFrame'),
    ...cmd('SetView'),
    ...cmd('SetPresentationContract'),
    ...cmd('Nop'),
    ...cmd('EndFrame'),
  ]);
  const full = buildFrame(ir, {
    abiVersion: ir.abi.version,
    flags: 0,
    frameId: 2,
    sequence: 1,
    resourceEpoch: 1,
    deadline: 1234,
    commandCount: 5,
    commandBytes: fullPayload.length,
  }, fullPayload);

  // debug frame: one DebugBootstrap record, header flag set/cleared below
  const debugStream = new Uint8Array([...cmd('BeginFrame'), ...cmd('DebugBootstrap')]);
  const debugWith = buildFrame(ir, {
    abiVersion: ir.abi.version, flags: 1, frameId: 3, sequence: 2, resourceEpoch: 1,
    deadline: 0, commandCount: 2, commandBytes: debugStream.length,
  }, debugStream);

  // ABI v2: implemented debug commands execute in the shell (plan W2.1/D8);
  // they still require the header debug flag like every 0xF00n opcode.
  const blitStream = new Uint8Array([
    ...cmd('BeginFrame'), ...cmd('DebugFrameBlit'), ...cmd('DebugRumble'), ...cmd('EndFrame'),
  ]);
  const blitOk = buildFrame(ir, {
    abiVersion: ir.abi.version, flags: 1, frameId: 5, sequence: 3, resourceEpoch: 1,
    deadline: 0, commandCount: 4, commandBytes: blitStream.length,
  }, blitStream);
  const blitNoFlag = buildFrame(ir, {
    abiVersion: ir.abi.version, flags: 0, frameId: 6, sequence: 4, resourceEpoch: 1,
    deadline: 0, commandCount: 4, commandBytes: blitStream.length,
  }, blitStream);

  const empty = buildFrame(ir, {
    abiVersion: ir.abi.version, flags: 0, frameId: 4, sequence: 3, resourceEpoch: 1,
    deadline: 0, commandCount: 0, commandBytes: 0,
  }, new Uint8Array(0));

  // Offsets INTO the full stream, measured from the records themselves.
  const spcModeOffset = cmd('BeginFrame').length + cmd('SetView').length + 16;

  const cases: { name: string; packet: Uint8Array }[] = [
    { name: 'valid_minimal', packet: base },
    { name: 'valid_all_implemented', packet: full },
    { name: 'valid_empty_frame', packet: empty },
    { name: 'valid_debug_with_flag', packet: debugWith },
    { name: 'valid_debug_blit_rumble', packet: blitOk },
    { name: 'debug_blit_without_flag', packet: blitNoFlag },

    // header-level corruption (no re-seal: the corruption IS the test)
    { name: 'bad_magic', packet: mutate32(base, 0, 0xdeadbeef, false) },
    { name: 'bad_abi_version', packet: mutate16(base, OFF_ABI_VERSION, ir.abi.version + 1, false) },
    { name: 'bad_header_crc', packet: mutate(base, 33, 0xff) },
    { name: 'reserved_frame_flag', packet: mutate16(base, OFF_FLAGS, 0x0008, true) },

    // lengths
    { name: 'misaligned_command_bytes', packet: mutate32(base, OFF_COMMAND_BYTES, 100, false) },
    { name: 'count_exceeds_capacity', packet: mutate32(base, OFF_COMMAND_COUNT, 7, false) },
    { name: 'oversize_command_bytes', packet: mutate32(base, OFF_COMMAND_BYTES, slotBytes, false) },
    { name: 'packet_truncated', packet: base.slice(0, base.length - 8) },
    { name: 'packet_too_short', packet: base.slice(0, 10) },

    // payload corruption
    { name: 'bad_payload_crc', packet: mutate(base, base.length - 1, base[base.length - 1]! ^ 0xff) },
    { name: 'payload_bit_flip', packet: mutate(base, HEADER_BYTES + 40, 0xff) },

    // record-level (re-sealed so CRC checks pass and record walk is reached)
    // base stream: BeginFrame@0 (32B), Nop@32 (16B), EndFrame@48 (32B)
    { name: 'unknown_opcode', packet: reframe(mutate16(base, HEADER_BYTES + 48, 0x0500, false)) },
    { name: 'record_size_mismatch', packet: reframe(mutate16(base, HEADER_BYTES + 2, 48, false)) },
    { name: 'record_flags_nonzero', packet: reframe(mutate32(base, HEADER_BYTES + 8, 0x1, false)) },
    { name: 'record_reserved_nonzero', packet: reframe(mutate32(base, HEADER_BYTES + 12, 0x1, false)) },
    { name: 'payload_pad_nonzero', packet: reframe(mutate(base, HEADER_BYTES + 48 + 16 + 12, 0x42)) },

    // enum range (ABI v2, capture_format.md 3.2 step 7). The SPC payload's
    // `mode` (video_mode u8) is the first payload byte of the third record, so
    // its stream offset is the two records before it plus one record header.
    //
    // THIS WAS A HARDCODED 128, spelled out in a comment as
    // "SetView@32 (96B), SetPresentationContract@128". Owner ruling R63 grew
    // SetView to 112, moving SPC to 144 -- so the mutation landed inside
    // SetView's payload instead of on the enum, and the case reported
    // ZH_ABI_UNKNOWN_OPCODE (7) where it asserts ZH_ABI_BAD_VALUE (9). A
    // corpus that mutates by hardcoded offset stops testing what it names the
    // moment any earlier record changes size, and it fails LOUDLY only because
    // the expected error is recorded; a case whose mutation silently landed on
    // a pad byte would have gone on passing while testing nothing.
    { name: 'enum_out_of_range', packet: reframe(mutate(full, HEADER_BYTES + spcModeOffset, 0x07)) },

    // walk-level
    { name: 'count_mismatch', packet: mutate32(base, OFF_COMMAND_COUNT, 2, true) },
    { name: 'debug_without_flag', packet: mutate16(debugWith, OFF_FLAGS, 0x0000, true) },

    // m2 (review RUN-20260814-1912): a record STRADDLING the frame end.
    // Everything header-level passes (48 B aligned stream, both CRCs valid —
    // the stream really is BeginFrame + a Nop record whose declared
    // record_bytes is widened to 32 so it runs past command_bytes) and the
    // walk reaches record 2. NOTE, discovered while writing this case: under
    // the frozen fail-safe order the straddling record trips check 2
    // ("lengths ... out of bounds", off + record_bytes > command_bytes) and
    // returns ZH_ABI_BAD_LENGTH (4) — ZH_ABI_TRUNCATED (11) is DEFENSIVE
    // walk code that is unreachable while command_bytes and every
    // record_bytes are 16-aligned (both are enforced before any 11 site).
    // The exact code is asserted, not assumed: expectedError below comes
    // from the oracle validator itself.
    {
      name: 'record_straddles_frame_end',
      packet: (() => {
        const nop = cmd('Nop');
        nop[2] = 32; // record_bytes: 16 -> 32 (wider than the 16 B remaining)
        nop[3] = 0;
        return buildFrame(ir, {
          abiVersion: ir.abi.version, flags: 0, frameId: 7, sequence: 5,
          resourceEpoch: 1, deadline: 0, commandCount: 2, commandBytes: 48,
        }, new Uint8Array([...cmd('BeginFrame'), ...nop]));
      })(),
    },
  ];

  return cases.map((c) => {
    const r = validateFrame(ir, c.packet, slotBytes);
    return { name: c.name, packet: c.packet, expectedError: r.error };
  });
}

/** Serialize the corpus: "ZCOR" magic, version, count, then per case. */
export function corpusBinary(cases: readonly CorpusCase[]): Uint8Array {
  const enc = new TextEncoder();
  let size = 4 + 2 + 2 + 4;
  for (const c of cases) {
    size += 2 + enc.encode(c.name).length + 4 + c.packet.length + 4;
  }
  const out = new Uint8Array(size);
  const dv = new DataView(out.buffer);
  dv.setUint32(0, 0x524f435a, true); // 'Z','C','O','R' LE
  dv.setUint16(4, 1, true);
  dv.setUint16(6, 0, true);
  dv.setUint32(8, cases.length, true);
  let off = 12;
  for (const c of cases) {
    const name = enc.encode(c.name);
    dv.setUint16(off, name.length, true);
    off += 2;
    out.set(name, off);
    off += name.length;
    dv.setUint32(off, c.packet.length, true);
    off += 4;
    out.set(c.packet, off);
    off += c.packet.length;
    dv.setUint32(off, c.expectedError, true);
    off += 4;
  }
  return out;
}
