// crc32c.ts — CRC-32C (Castagnoli), CRC-32/ISCSI parameter set
// (spec/capture_format.md 2): poly 0x82F63B78 reflected, init/xorout
// 0xFFFFFFFF. Same algorithm is emitted into the generated C++/TS/SV; this
// copy is the generator-internal oracle used to seal golden frames.

export const CRC32C_TABLE: readonly number[] = (() => {
  const t = new Array<number>(256);
  for (let i = 0; i < 256; i++) {
    let c = i;
    for (let k = 0; k < 8; k++) {
      c = (c >>> 1) ^ (0x82f63b78 & (c & 1 ? 0xffffffff : 0));
    }
    t[i] = c >>> 0;
  }
  return t;
})();

/**
 * Running form (spec/capture_format.md 2): pass crc=0 for a fresh message or
 * the previous return value to continue; returns the finalized CRC
 * (xorout applied). Stored little-endian per the ABI-wide rule.
 */
export function crc32c(crc: number, buf: Uint8Array, off = 0, len = buf.length - off): number {
  let c = (crc ^ 0xffffffff) >>> 0;
  for (let i = 0; i < len; i++) {
    c = (CRC32C_TABLE[(c ^ buf[off + i]!) & 0xff]! ^ (c >>> 8)) >>> 0;
  }
  return (c ^ 0xffffffff) >>> 0;
}

/** CRC-32C test vectors (spec/capture_format.md 2.1 — normative). */
export const CRC32C_VECTORS: readonly { name: string; data: Uint8Array; crc: number }[] = [
  { name: 'empty', data: new Uint8Array(0), crc: 0x00000000 },
  {
    name: '123456789', data: new Uint8Array([0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39]),
    crc: 0xe3069283,
  },
  { name: '32x00', data: new Uint8Array(32).fill(0x00), crc: 0x8a9136aa },
  { name: '32xFF', data: new Uint8Array(32).fill(0xff), crc: 0x62a8ab43 },
  { name: '00..1F', data: Uint8Array.from({ length: 32 }, (_, i) => i), crc: 0x46dd794e },
  { name: '1F..00', data: Uint8Array.from({ length: 32 }, (_, i) => 31 - i), crc: 0x113fdb5c },
];

/**
 * Check constant (verified empirically; see capture_format.md 2.1): for every
 * message, crc32c(0, message ‖ LE(crc32c(0, message))) === 0x48674BC7.
 * Equivalently the init-seeded register without xorout ends at the RevEng
 * catalogue residue 0xB798B438. The P5 recon's 0x1C2D19ED is NOT
 * reproducible in this parameterization and was corrected (in-suite evidence).
 */
export const CRC32C_CHECK_CONSTANT = 0x48674bc7;
export const CRC32C_REGISTER_RESIDUE = 0xb798b438;
