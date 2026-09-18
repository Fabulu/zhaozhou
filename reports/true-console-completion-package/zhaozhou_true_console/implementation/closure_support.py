"""Offline completion support, not a replacement for the repository's oracles.

Python 3.10+, standard library only. Implements:
* the ruled particle128 packing (qformats.md section 10);
* survivor-first, whole-child-group admission as a proposed transaction model;
* upload descriptor validation and a drain-before-publication transaction model;
* physical-shape M10K planning and necessary provider-calendar checks;
* a fail-closed completion ledger checker.
No function claims FPGA area, timing, complete ISA semantics or board validation.
"""
from __future__ import annotations
from dataclasses import dataclass, field
from enum import IntEnum
from pathlib import Path
from typing import Iterable, Mapping, Sequence
import argparse
import hashlib
import json
import math
import sys

LAYOUT = (
    ('x', 0, 18, True), ('y', 18, 18, True), ('z', 36, 18, True),
    ('vx', 54, 11, True), ('vy', 65, 11, True), ('vz', 76, 11, True),
    ('age', 87, 10, False), ('species', 97, 7, False),
    ('size', 104, 6, False), ('spin', 110, 6, False),
    ('flags', 116, 4, False), ('variation', 120, 8, False),
)


def checked_uint(value: int, bits: int, name: str) -> int:
    if type(value) is not int or not 0 <= value < (1 << bits):
        raise ValueError(f'{name} must be an unsigned {bits}-bit integer')
    return value


def pack_particle(values: Mapping[str, int]) -> bytes:
    """Pack exactly 16 little-endian bytes. Does not invent update arithmetic."""
    if set(values) != {x[0] for x in LAYOUT}:
        raise ValueError('particle fields must exactly match the ruled layout')
    word = 0
    for name, shift, width, signed in LAYOUT:
        value = values[name]
        low, high = (-(1 << (width-1)), (1 << (width-1))-1) if signed else (0, (1 << width)-1)
        if type(value) is not int or not low <= value <= high:
            raise ValueError(f'{name} outside [{low}, {high}]')
        if name == 'flags' and value & 8:
            raise ValueError('reserved particle flag bit 3 must be zero')
        word |= (value & ((1 << width)-1)) << shift
    return word.to_bytes(16, 'little')


def unpack_particle(data: bytes) -> dict[str, int]:
    if len(data) != 16:
        raise ValueError('particle record must contain exactly 16 bytes')
    word = int.from_bytes(data, 'little')
    result = {}
    for name, shift, width, signed in LAYOUT:
        raw = (word >> shift) & ((1 << width)-1)
        result[name] = raw - (1 << width) if signed and raw & (1 << (width-1)) else raw
    if result['flags'] & 8:
        raise ValueError('reserved particle flag bit 3 is nonzero')
    return result


@dataclass(frozen=True)
class ParticleRecord:
    stable_id: int  # Adapter input, NOT a new field silently inserted into particle128.
    payload: bytes
    def __post_init__(self):
        checked_uint(self.stable_id, 64, 'stable_id')
        unpack_particle(self.payload)


@dataclass(frozen=True)
class SpawnGroup:
    parent_order: int
    event_order: int
    children: tuple[ParticleRecord, ...]
    def __post_init__(self):
        if self.parent_order < 0 or not 0 <= self.event_order < 4:
            raise ValueError('invalid parent/event order')


@dataclass(frozen=True)
class TickResult:
    records: tuple[ParticleRecord, ...]
    children_requested: int
    children_emitted: int
    children_refused: int
    malformed_groups: int


def finish_particle_tick(survivors: Sequence[ParticleRecord], groups: Sequence[SpawnGroup],
                         capacity: int) -> TickResult:
    """Proposed whole-group, prefix-priority admission policy.

    Update/collision are external exact oracles. Groups must already be in
    parent/event order. After a legal group cannot fit, later legal groups are
    refused even if smaller: this preserves the strict-prefix policy documented
    in the handoff. Confirm that policy against current owner rulings before
    production adoption. A >16-child group is malformed, refused atomically.
    """
    if type(capacity) is not int or capacity < 0 or len(survivors) > capacity:
        raise ValueError('cannot evict survivors to meet capacity')
    keys = [(g.parent_order, g.event_order) for g in groups]
    if keys != sorted(keys) or len(set(keys)) != len(keys):
        raise ValueError('groups must have unique, ordered parent/event keys')
    output = list(survivors)
    requested = emitted = refused = malformed = 0
    exhausted = False
    for g in groups:
        count = len(g.children)
        requested += count
        if count > 16:
            malformed += 1
            refused += count
            continue
        if count == 0:
            continue
        if exhausted or len(output) + count > capacity:
            exhausted = True
            refused += count
        else:
            output.extend(g.children)
            emitted += count
    ids = [p.stable_id for p in output]
    if len(set(ids)) != len(ids):
        raise ValueError('duplicate stable particle identity')
    return TickResult(tuple(output), requested, emitted, refused, malformed)


class UploadVerdict(IntEnum):
    OK = 0
    UNALIGNED = 1
    ZERO_LENGTH = 2
    OUTSIDE_GUARD = 3
    EPOCH_STALE = 4
    CRC_FAIL = 5
    SOURCE_OUTSIDE_ARENA = 6
    SOURCE_UNREACHABLE = 7


def upload_verdict(hps_addr: int, vram_addr: int, length: int,
                   source_base: int, source_bytes: int, dest_base: int, dest_bytes: int,
                   request_epoch: int, current_epoch: int) -> UploadVerdict:
    """Mirror zref_mem_upload.hpp predicate order, with unbounded host addition."""
    checked_uint(hps_addr, 64, 'hps_addr')
    for name, value in [('vram_addr', vram_addr), ('length', length),
                        ('source_base', source_base), ('source_bytes', source_bytes),
                        ('dest_base', dest_base), ('dest_bytes', dest_bytes)]:
        checked_uint(value, 32, name)
    checked_uint(request_epoch, 16, 'request_epoch')
    checked_uint(current_epoch, 16, 'current_epoch')
    if length == 0:
        return UploadVerdict.ZERO_LENGTH
    if (hps_addr | vram_addr | length) & 63:
        return UploadVerdict.UNALIGNED
    if hps_addr >> 32:
        return UploadVerdict.SOURCE_UNREACHABLE
    if not source_base <= hps_addr or hps_addr + length > source_base + source_bytes:
        return UploadVerdict.SOURCE_OUTSIDE_ARENA
    if not dest_base <= vram_addr or vram_addr + length > dest_base + dest_bytes:
        return UploadVerdict.OUTSIDE_GUARD
    if request_epoch != current_epoch:
        return UploadVerdict.EPOCH_STALE
    return UploadVerdict.OK


@dataclass
class UploadTransaction:
    """Transaction-level model; retirement is external memory acknowledgement.

    Abort quarantines the destination until all previously issued writes drain.
    reset() is deliberately absent: global reset requires a transport-drain
    contract, not forgetting outstanding writes. Late foreign responses do not
    credit this transaction. Models one transaction, not a full DMA engine.
    """
    token: int
    expected_words: int
    epoch: int
    generation: int
    destination_slot: int
    issued: int = 0
    retired: int = 0
    issue_closed: bool = False
    crc_ok: bool | None = None
    failed: bool = False
    published: bool = False
    _seen: set[int] = field(default_factory=set, repr=False)

    def __post_init__(self):
        if type(self.expected_words) is not int or self.expected_words <= 0:
            raise ValueError('positive integral word count required')
        for name, value in [('epoch', self.epoch), ('generation', self.generation)]:
            checked_uint(value, 16, name)

    def issue(self, count: int = 1) -> None:
        if type(count) is not int or count < 0 or self.issue_closed or self.failed or self.published:
            raise ValueError('issue after transaction closure or invalid count')
        if self.issued + count > self.expected_words:
            raise ValueError('issued too many words')
        self.issued += count

    def retire(self, token: int, first_word: int, count: int = 1) -> bool:
        if token != self.token:
            return False
        if (type(count) is not int or type(first_word) is not int or count < 1
                or first_word < 0 or first_word + count > self.issued):
            raise ValueError('retirement not backed by issued writes')
        indices = set(range(first_word, first_word+count))
        if self._seen & indices:
            raise ValueError('duplicate write credit')
        self._seen |= indices
        self.retired += count
        return True

    def close_issue(self) -> None:
        if self.issued != self.expected_words and not self.failed:
            raise ValueError('successful issuer cannot stop early')
        self.issue_closed = True

    def complete_crc(self, ok: bool) -> None:
        if type(ok) is not bool:
            raise ValueError('CRC verdict must be boolean')
        if self.crc_ok is not None:
            raise ValueError('CRC already supplied')
        self.crc_ok = bool(ok)
        if not ok:
            self.failed = True

    def abort(self) -> None:
        if self.published:
            raise ValueError('cannot abort already published generation')
        self.failed = True
        self.issue_closed = True  # Caller has cancelled unsent issues first.

    @property
    def drained(self) -> bool:
        return self.issue_closed and self.retired == self.issued

    @property
    def reclaimable(self) -> bool:
        return self.failed and self.drained and not self.published

    def publish(self, current_epoch: int, new_slot_unpublished: bool,
                old_mapping: tuple[int, int]) -> tuple[int, int]:
        checked_uint(current_epoch, 16, 'current_epoch')
        if type(new_slot_unpublished) is not bool:
            raise ValueError('slot visibility must be boolean')
        if self.published:
            raise ValueError('duplicate publication')
        if current_epoch != self.epoch:
            self.failed = True
        if not (self.drained and self.issued == self.expected_words and self.crc_ok is True
                and not self.failed and new_slot_unpublished
                and self.destination_slot != old_mapping[0]):
            raise ValueError('publication preconditions not met')
        self.published = True
        return (self.destination_slot, self.generation)


# Single/simple-dual-port shapes; true-dual-port has no 256x40 mode.
SDP_SHAPES = ((256,40),(512,20),(1024,10),(2048,5),(4096,2),(8192,1))
TDP_SHAPES = ((512,20),(1024,10),(2048,5),(4096,2),(8192,1))


def memory_plan(depth: int, width: int, mode: str = 'sdp', copies: int = 1) -> dict:
    """Shape estimate, never a fitted count. Port-copy factor must be supplied."""
    if any(type(v) is not int or v <= 0 for v in (depth, width, copies)):
        raise ValueError('depth, width and copies must be positive integers')
    if mode not in ('sdp', 'tdp'):
        raise ValueError('mode must be sdp or tdp')
    shapes = SDP_SHAPES if mode == 'sdp' else TDP_SHAPES
    choices = []
    for d,w in shapes:
        banks = (depth+d-1)//d
        slices = (width+w-1)//w
        blocks = banks*slices*copies
        choices.append((blocks, banks, slices, d,w))
    blocks,banks,slices,d,w = min(choices)
    return dict(evidence='shape_estimate_not_fit', blocks=blocks, mode=mode,
                primitive_depth=d, primitive_width=w, depth_banks=banks,
                width_slices=slices, physical_copies=copies,
                logical_bits=depth*width*copies,
                physical_capacity_bits=blocks*10240)


def provider_bound(jobs: int, issue_slots: int, lanes: int, window_cycles: int,
                   overhead_cycles: int = 0) -> dict:
    """Necessary capacity test ONLY. Passing does not prove a legal schedule."""
    vals = (jobs,issue_slots,lanes,window_cycles,overhead_cycles)
    if any(type(v) is not int for v in vals) or min(jobs,issue_slots,overhead_cycles) < 0:
        raise ValueError('nonnegative integral demand required')
    if lanes <= 0 or window_cycles <= 0:
        raise ValueError('positive lanes and window required')
    cycles = (jobs*issue_slots+lanes-1)//lanes+overhead_cycles
    return dict(necessary_cycles=cycles, window_cycles=window_cycles,
                impossible_even_without_stalls=cycles > window_cycles,
                schedule_proven=False)


class LedgerError(ValueError):
    pass


def check_ledger(doc: dict, root: Path | None = None, release: bool = False) -> list[str]:
    """Check honest classification; optional declared evidence/file existence.

    This DOES NOT elaborate RTL or discover functional reachability. Reachability
    and liveness require external reports/tests. Facts cannot be self-certified
    by inventing a JSON row. Unknown mandatory scope and manual-review items
    fail release even when names happen to match existing files.
    """
    if doc.get('schema') != 1 or not isinstance(doc.get('capabilities'), list):
        raise LedgerError('schema=1 and capabilities[] required')
    if not isinstance(doc.get('owners'), dict):
        raise LedgerError('owners{} required')
    ids: set[str] = set()
    problems: list[str] = []
    required = {'id','policy','state','owner','implementation','evidence','notes'}
    policies = {'required','optional_selected','deferred','excluded','needs_reconciliation'}
    states = {'absent','candidate','unit_verified','integrated','mapped','fitted','hardware_proven','not_applicable','unknown'}
    for row in doc['capabilities']:
        if not isinstance(row,dict) or not required <= row.keys():
            raise LedgerError('incomplete capability row')
        name = row['id']
        if not isinstance(name,str) or not name or name in ids:
            raise LedgerError('duplicate/invalid capability id')
        ids.add(name)
        if row['policy'] not in policies or row['state'] not in states:
            raise LedgerError(f'{name}: unsupported policy/state')
        active = row['policy'] in ('required','optional_selected')
        if row['policy'] == 'needs_reconciliation':
            problems.append(f'{name}: requirement status unresolved')
        if active and row['owner'] not in doc['owners']:
            problems.append(f'{name}: no physical/software owner')
        if active and row['state'] in ('absent','unknown','candidate','unit_verified','not_applicable'):
            problems.append(f"{name}: not connected-and-proven ({row['state']})")
        if active and not row['implementation']:
            problems.append(f'{name}: no implementation path')
        if active and not row['evidence']:
            problems.append(f'{name}: no supporting evidence')
        if row['state'] in ('integrated','mapped','fitted','hardware_proven'):
            need = {'functional_path','negative_control','scope_digest'}
            if not need <= row['evidence'].keys():
                problems.append(f'{name}: missing integration/liveness evidence fields')
        if root is not None:
            base = root.resolve()
            for path in row['implementation']:
                target = (base/path).resolve()
                if not target.is_relative_to(base) or not target.is_file():
                    problems.append(f'{name}: missing/outside implementation file {path}')
    for name, owner in doc['owners'].items():
        if owner.get('kind') not in ('rtl','software','mixed','platform'):
            raise LedgerError(f'{name}: invalid owner kind')
        area = owner.get('fitted_alms')
        if area is not None:
            if not isinstance(area,(float,int)) or area < 0:
                raise LedgerError('invalid fitted ALM count')
            if owner.get('area_evidence') != 'matched_fit':
                raise LedgerError(f'{name}: fitted ALMs require matched_fit evidence')
        if owner.get('candidate_savings_alms') is not None:
            raise LedgerError('do not subtract speculative savings in the completion ledger')
    # A complete capability list alone is not a release certificate.
    # These references need external review; this checker cannot verify their truth.
    release_evidence = doc.get('release_evidence', {})
    for requirement in ('function_complete', 'selected_scope_complete', 'target_fit',
                        'timing', 'workload', 'board'):
        claim = release_evidence.get(requirement, {})
        if not (isinstance(claim, dict) and claim.get('status') == 'pass'
                and claim.get('path') and claim.get('scope_digest')):
            problems.append(f'release: {requirement} not evidenced')
        elif root is not None:
            target = (root.resolve() / claim['path']).resolve()
            if not target.is_relative_to(root.resolve()) or not target.is_file():
                problems.append(f'release: {requirement} evidence file missing/outside root')
    if release and problems:
        raise LedgerError('\n'.join(problems))
    return problems


def snapshot_files(root: Path, paths: Iterable[str]) -> dict:
    """Hash explicitly supplied files; no claim that this is the RTL closure."""
    base = root.resolve()
    rows = []
    for name in sorted(set(paths)):
        p = (base/name).resolve()
        if not p.is_relative_to(base) or not p.is_file():
            raise ValueError(f'missing/outside path {name}')
        data = p.read_bytes()
        rows.append(dict(path=name, bytes=len(data), sha256=hashlib.sha256(data).hexdigest()))
    wire = json.dumps(rows,sort_keys=True,separators=(',',':')).encode()
    return dict(files=rows, manifest_sha256=hashlib.sha256(wire).hexdigest(),
                scope='explicit_file_list_not_elaboration_proof')


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command',required=True)
    ledger = sub.add_parser('ledger')
    ledger.add_argument('json_file',type=Path)
    ledger.add_argument('--root',type=Path)
    ledger.add_argument('--release',action='store_true')
    mem = sub.add_parser('memory')
    mem.add_argument('depth',type=int); mem.add_argument('width',type=int)
    mem.add_argument('--mode',choices=['sdp','tdp'],default='sdp')
    mem.add_argument('--copies',type=int,default=1)
    args = parser.parse_args()
    try:
        if args.command == 'memory':
            print(json.dumps(memory_plan(args.depth,args.width,args.mode,args.copies),indent=2))
        else:
            issues=check_ledger(json.loads(args.json_file.read_text(encoding='utf-8')),args.root,args.release)
            print(json.dumps(dict(release_ready=not issues, blockers=issues),indent=2))
        return 0
    except (ValueError,OSError,KeyError,TypeError) as e:
        print(str(e),file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
