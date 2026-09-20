#!/usr/bin/env python3
"""capture_diff.py -- classify every differing byte between two .zcap captures.

WHY THIS EXISTS. Any edit to `spec/commands.zidl` moves `ZHAO_ZIDL_SHA256` and
forces all five golden captures to be regenerated through their real producers
(`demo_duo_markers --write` is 600 Duo frames, about an hour). The acceptance
criterion for such a regeneration is NOT "the suite is green" -- it is:

    each capture differs from its predecessor ONLY in the container CRC and the
    two 32-byte sha fields of its ABI_INFO section.

Anything else that moved is a real behavioural change, and it must be explained
or reverted rather than waved through. A green suite cannot answer that
question, because the suite re-derives the same CRCs it is checking.

This probe is COMMITTED on purpose. The same measurement was going to be done
once by hand and thrown away, which is how a number becomes unreproducible --
CLAUDE.md's "commit the probe". It is a COMPARISON instrument: it never decides
what a capture should contain, it only reports where two of them differ.

LAYOUT (spec/capture_format.md; mirrored in tools/abi-gen/src/zcap_build.ts).
Everything below is parsed structurally from the file, never hardcoded by
offset, because section counts differ between captures and a hardcoded offset
would silently classify the wrong bytes as benign -- the flattering direction.

  header, 32 B      magic@0 u32 | format_version@4 u16 | flags@6 u16
                    | header_crc@8 u32 (crc32c over bytes [0,8))
                    | section_count@12 u32 | section_table_offset@16 u32
                    | section_entry_size@20 u32 | total_file_length@24 u64
  section entry,    type@0 u16 | version@2 u16 | crc_present@4 u16 | rsv@6 u16
  32 B each         | body_offset@8 u64 | body_length@16 u64
                    | body_crc32c@24 u32 | rsv@28 u32
  ABI_INFO body     abi_version@0 u32 | zcap_schema_version@4 u32
  (section 0x0001)  | generator_name@8 16 B | generator_sha256@24 32 B
                    | zidl_sha256@56 32 B

`generator_sha256` is the ABI IDENTITY hash: tools/abi-gen/src/layout.ts's
`identityText()` includes every enum's entries, so adding an enum member moves
it. `zidl_sha256` is the hash of the .zidl source text, so a COMMENT-ONLY edit
moves that one alone. Both are expected to move; both are reported by name.

USAGE
  python tools/abi/capture_diff.py OLD.zcap NEW.zcap
  python tools/abi/capture_diff.py --git-head captures/golden/wave2/duo.zcap
  python tools/abi/capture_diff.py --selftest

Exit 0 = every differing byte is benign (container CRCs + the two sha fields).
Exit 1 = at least one differing byte lies outside them, or the files differ in
         structure (length, section count, section layout). Exit 2 = usage.
"""

from __future__ import annotations

import argparse
import struct
import subprocess
import sys
from dataclasses import dataclass

ZCAP_MAGIC = 0x5041435A
ZCAP_HEADER_BYTES = 32
ZCAP_SECTION_ENTRY_BYTES = 32
SECTION_ABI_INFO = 0x0001

# ABI_INFO body field offsets
ABI_INFO_GENERATOR_SHA_OFF = 24
ABI_INFO_ZIDL_SHA_OFF = 56
SHA256_BYTES = 32


@dataclass(frozen=True)
class Span:
    """A byte range [lo, hi) in the FILE, with the name it is allowed to have."""

    lo: int
    hi: int
    name: str

    def contains(self, off: int) -> bool:
        return self.lo <= off < self.hi


@dataclass(frozen=True)
class Section:
    index: int
    type_id: int
    body_offset: int
    body_length: int
    entry_offset: int


def parse_sections(blob: bytes) -> list[Section]:
    """Parse the section table. Raises ValueError on anything malformed."""
    if len(blob) < ZCAP_HEADER_BYTES:
        raise ValueError(f"shorter than a .zcap header ({len(blob)} B < {ZCAP_HEADER_BYTES})")
    magic, = struct.unpack_from("<I", blob, 0)
    if magic != ZCAP_MAGIC:
        raise ValueError(f"bad magic 0x{magic:08x} (expected 0x{ZCAP_MAGIC:08x})")
    count, table_off, entry_size = struct.unpack_from("<III", blob, 12)
    if entry_size != ZCAP_SECTION_ENTRY_BYTES:
        raise ValueError(f"section_entry_size {entry_size} != {ZCAP_SECTION_ENTRY_BYTES}")
    out: list[Section] = []
    for i in range(count):
        e = table_off + i * entry_size
        if e + entry_size > len(blob):
            raise ValueError(f"section table entry {i} runs past end of file")
        type_id, = struct.unpack_from("<H", blob, e + 0)
        body_off, = struct.unpack_from("<Q", blob, e + 8)
        body_len, = struct.unpack_from("<Q", blob, e + 16)
        if body_off + body_len > len(blob):
            raise ValueError(f"section {i} body runs past end of file")
        out.append(Section(i, type_id, body_off, body_len, e))
    return out


def benign_spans(blob: bytes) -> list[Span]:
    """The byte ranges a pure re-generation of the SAME capture may move.

    Deliberately narrow. Every other byte in the file is content, and content
    moving is the thing this probe exists to catch.
    """
    spans = [Span(8, 12, "header_crc32c")]
    for s in parse_sections(blob):
        spans.append(
            Span(
                s.entry_offset + 24,
                s.entry_offset + 28,
                f"section[{s.index}] type=0x{s.type_id:04x} body_crc32c",
            )
        )
        if s.type_id == SECTION_ABI_INFO:
            g = s.body_offset + ABI_INFO_GENERATOR_SHA_OFF
            z = s.body_offset + ABI_INFO_ZIDL_SHA_OFF
            spans.append(Span(g, g + SHA256_BYTES, "ABI_INFO.generator_sha256 (abi identity)"))
            spans.append(Span(z, z + SHA256_BYTES, "ABI_INFO.zidl_sha256"))
    return spans


def runs(offsets: list[int]) -> list[tuple[int, int]]:
    """Collapse sorted offsets into [lo, hi) runs, for readable reporting."""
    out: list[tuple[int, int]] = []
    for off in offsets:
        if out and off == out[-1][1]:
            out[-1] = (out[-1][0], off + 1)
        else:
            out.append((off, off + 1))
    return out


def compare(old: bytes, new: bytes, label: str) -> int:
    print(f"=== {label}")
    if old == new:
        print("  IDENTICAL (0 bytes differ)")
        print("  VERDICT: OK -- nothing moved")
        return 0

    if len(old) != len(new):
        print(f"  LENGTH CHANGED: {len(old)} -> {len(new)} B")
        print("  VERDICT: FAIL -- a pure sha/CRC refresh cannot change length")
        return 1

    try:
        old_secs, new_secs = parse_sections(old), parse_sections(new)
    except ValueError as exc:
        print(f"  UNPARSEABLE: {exc}")
        print("  VERDICT: FAIL -- cannot classify bytes in a malformed container")
        return 1

    old_layout = [(s.type_id, s.body_offset, s.body_length) for s in old_secs]
    new_layout = [(s.type_id, s.body_offset, s.body_length) for s in new_secs]
    if old_layout != new_layout:
        print(f"  SECTION LAYOUT CHANGED: {len(old_secs)} -> {len(new_secs)} sections")
        print("  VERDICT: FAIL -- section types/offsets/lengths must be identical")
        return 1

    spans = benign_spans(new)
    diff = [i for i in range(len(old)) if old[i] != new[i]]

    by_name: dict[str, list[int]] = {}
    rogue: list[int] = []
    for off in diff:
        for sp in spans:
            if sp.contains(off):
                by_name.setdefault(sp.name, []).append(off)
                break
        else:
            rogue.append(off)

    print(f"  {len(diff)} byte(s) differ, in {len(runs(diff))} run(s)")
    for name, offs in by_name.items():
        rs = ", ".join(f"[{lo}..{hi - 1}]" for lo, hi in runs(offs))
        print(f"    EXPECTED  {len(offs):4d} B  {name}  {rs}")

    if rogue:
        rs = ", ".join(f"[{lo}..{hi - 1}]" for lo, hi in runs(rogue))
        print(f"    ROGUE     {len(rogue):4d} B  OUTSIDE every benign span  {rs}")
        for lo, hi in runs(rogue)[:8]:
            print(f"      @{lo}: {old[lo:hi].hex()} -> {new[lo:hi].hex()}")
        print("  VERDICT: FAIL -- a real behavioural change, explain it or revert it")
        return 1

    print("  VERDICT: OK -- only container CRCs and the two sha fields moved")
    return 0


def git_show(rev_path: str, cwd: str | None) -> bytes:
    return subprocess.run(
        ["git", "show", rev_path], check=True, stdout=subprocess.PIPE, cwd=cwd
    ).stdout


def selftest() -> int:
    """Fire the detector. A probe that has never reported FAIL is a claim.

    Builds a minimal 1-section container, then mutates (a) a benign sha byte,
    which must PASS, and (b) a body byte outside every benign span, which must
    FAIL. Without (b) the OK verdict would be untested in the only direction
    that matters.
    """
    body = bytes(88)  # ABI_INFO body: 4+4+16+32+32
    blob = bytearray(ZCAP_HEADER_BYTES + ZCAP_SECTION_ENTRY_BYTES + len(body))
    struct.pack_into("<I", blob, 0, ZCAP_MAGIC)
    struct.pack_into("<H", blob, 4, 1)
    struct.pack_into("<III", blob, 12, 1, ZCAP_HEADER_BYTES, ZCAP_SECTION_ENTRY_BYTES)
    struct.pack_into("<Q", blob, 24, len(blob))
    e = ZCAP_HEADER_BYTES
    struct.pack_into("<H", blob, e + 0, SECTION_ABI_INFO)
    struct.pack_into("<Q", blob, e + 8, ZCAP_HEADER_BYTES + ZCAP_SECTION_ENTRY_BYTES)
    struct.pack_into("<Q", blob, e + 16, len(body))
    base = bytes(blob)

    body_off = ZCAP_HEADER_BYTES + ZCAP_SECTION_ENTRY_BYTES
    failures = 0

    benign = bytearray(base)
    benign[body_off + ABI_INFO_ZIDL_SHA_OFF] ^= 0xFF
    benign[e + 24] ^= 0xFF  # the section body CRC
    print("-- selftest 1: sha + CRC bytes only (must be OK)")
    if compare(base, bytes(benign), "selftest/benign") != 0:
        print("  SELFTEST FAILED: benign mutation reported FAIL")
        failures += 1

    rogue = bytearray(base)
    rogue[body_off + 0] ^= 0xFF  # abi_version -- content, not a sha or a CRC
    print("-- selftest 2: a content byte (must FAIL; this is the fire test)")
    if compare(base, bytes(rogue), "selftest/rogue") == 0:
        print("  SELFTEST FAILED: rogue mutation reported OK -- detector is blind")
        failures += 1

    print(f"== selftest {'PASSED' if failures == 0 else 'FAILED'} ({failures} problem(s))")
    return 1 if failures else 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("paths", nargs="*", help="OLD.zcap NEW.zcap, or (with --git-head) paths")
    ap.add_argument("--git-head", action="store_true", help="compare each path against its committed HEAD blob")
    ap.add_argument("--rev", default="HEAD", help="revision for --git-head (default HEAD)")
    ap.add_argument("--repo", default=None, help="repository directory for git")
    ap.add_argument("--selftest", action="store_true", help="fire the detector and exit")
    args = ap.parse_args()

    if args.selftest:
        return selftest()

    rc = 0
    if args.git_head:
        if not args.paths:
            ap.error("--git-head needs at least one path")
        for p in args.paths:
            rel = p.replace("\\", "/")
            try:
                old = git_show(f"{args.rev}:{rel}", args.repo)
            except subprocess.CalledProcessError:
                print(f"=== {rel}\n  NOT IN {args.rev} (new file)\n  VERDICT: FAIL -- no predecessor to compare")
                rc = 1
                continue
            with open(p, "rb") as fh:
                new = fh.read()
            rc |= compare(old, new, f"{rel}  ({args.rev} -> working tree)")
        return rc

    if len(args.paths) != 2:
        ap.error("need exactly two paths (or --git-head, or --selftest)")
    with open(args.paths[0], "rb") as fh:
        old = fh.read()
    with open(args.paths[1], "rb") as fh:
        new = fh.read()
    return compare(old, new, f"{args.paths[0]} -> {args.paths[1]}")


if __name__ == "__main__":
    sys.exit(main())
