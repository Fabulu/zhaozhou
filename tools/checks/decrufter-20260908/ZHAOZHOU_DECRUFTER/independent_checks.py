#!/usr/bin/env python3
"""Source-derived finite models for the Zhaozhou decrufting brief.

These checks are NOT repository RTL simulation, formal verification, or Quartus.
Run with Python 3.10+: python independent_checks.py
Only the Python standard library is needed. Results are reproducible.
"""
from __future__ import annotations
from collections import deque
from dataclasses import dataclass
import itertools
import json
from pathlib import Path
import random

RESULTS: dict[str, object] = {}

@dataclass(frozen=True)
class Packet:
    token: int
    data: int
    meta: int

class ReadJoinModel:
    """Models r1 plus the directly connected metajoin read register.

    Current RTL reloads read_q on every active edge even when launch is false.
    Repaired model reloads it only on an accepted read. Valid is owned by r1.
    """
    def __init__(self, mem: dict[int, int], repaired: bool):
        self.mem = mem
        self.repaired = repaired
        self.v = False
        self.token = 0
        self.data = 0
        self.meta = 0
    def step(self, offer: tuple[int, int] | None, ready: bool,
             idle_address: int = 0, reset: bool = False):
        if reset:
            self.v = False
            return False, None
        out = Packet(self.token, self.data, self.meta) if self.v and ready else None
        room = not self.v or ready
        accept = offer is not None and room
        address = offer[0] if offer else idle_address
        if room:
            self.v = offer is not None
            if offer:
                self.token, self.data = offer
        if not self.repaired or accept:
            self.meta = self.mem[address]
        return accept, out

def stalled_join_counterexample():
    mem = {0: 0, 10: 0x111, 20: 0x222}
    schedule = [((10, 0xAAA), False), ((20, 0xBBB), False),
                ((20, 0xBBB), True), (None, True)]
    streams = {}
    traces = {}
    for repaired in (False, True):
        model = ReadJoinModel(mem, repaired)
        outputs, trace = [], []
        for edge, (offer, ready) in enumerate(schedule):
            accept, out = model.step(offer, ready)
            if out:
                outputs.append(out)
            trace.append({'edge': edge, 'offered': offer, 'downstream_ready': ready,
                          'accepted': accept, 'emitted': out.__dict__ if out else None,
                          'post_edge_held': {'valid': model.v, 'token': model.token,
                                            'data': model.data, 'meta': model.meta}})
        streams[repaired] = outputs
        traces['repaired' if repaired else 'current'] = trace
    assert streams[False][0] == Packet(10, 0xAAA, 0x222)
    assert streams[True] == [Packet(10, 0xAAA, 0x111), Packet(20, 0xBBB, 0x222)]
    assert len(streams[False]) == len(streams[True]) == 2
    RESULTS['J0_stalled_join_counterexample'] = {
        'result': 'PASS: current equations produce wrong metadata; accepted/emitted counts still agree',
        'traces': traces}

def repaired_join_random():
    total_accept = total_emit = resets = hold_checks = cancelled = 0
    for seed in range(32):
        rng = random.Random(seed + 0xDEC0)
        mem = {i: rng.getrandbits(40) for i in range(257)}
        model = ReadJoinModel(mem, True)
        expected = deque()
        pending = None
        for cyc in range(5000):
            if rng.randrange(1200) == 0:
                model.step(None, False, reset=True)
                cancelled += len(expected)
                expected.clear(); pending = None; resets += 1
                continue
            if pending is None and rng.random() < .83:
                pending = (rng.randrange(1, 257), rng.getrandbits(64))
            ready = bool(rng.getrandbits(1)) and not (100 < cyc % 311 < 140)
            before = (model.v, model.token, model.data, model.meta)
            acc, out = model.step(pending, ready, idle_address=rng.randrange(257))
            if out:
                assert expected and expected.popleft() == out
                total_emit += 1
            if acc:
                assert pending is not None
                expected.append(Packet(pending[0], pending[1], mem[pending[0]]))
                total_accept += 1; pending = None
            if before[0] and not ready:
                assert (model.v, model.token, model.data, model.meta) == before
                hold_checks += 1
            assert len(expected) == int(model.v)
        if model.v:
            _, out = model.step(None, True)
            assert expected.popleft() == out
            total_emit += 1
        assert not expected
    assert total_accept == total_emit + cancelled
    RESULTS['J1_repaired_join_random'] = {
        'result': 'PASS', 'seeds': 32, 'cycles_per_seed': 5000,
        'accepted': total_accept, 'emitted': total_emit,
        'reset_events': resets, 'cancelled_records': cancelled, 'whole_record_hold_checks': hold_checks}

def axis_queue_induction():
    # push means accepted AND nonzero; every nonempty queue pops each clock.
    # All 2^12 push schedules, followed by drain, include arbitrary zero/idle gaps.
    cases = 0; max_depth = 0
    for pushes in itertools.product((False, True), repeat=12):
        q = [deque(), deque()]
        for cycle, push in enumerate((*pushes, False, False)):
            assert list(q[0]) == list(q[1])
            popped = []
            for axis in (0, 1):
                popped.append(q[axis].popleft() if q[axis] else None)
                if push:
                    q[axis].append(cycle % 16)
            assert popped[0] == popped[1]
            max_depth = max(max_depth, len(q[0]))
            assert len(q[0]) <= 1 and list(q[0]) == list(q[1])
        assert not q[0] and not q[1]
        cases += 1
    RESULTS['P0_axis_queue_lockstep'] = {'result': 'PASS', 'schedules': cases,
        'max_queue_depth': max_depth,
        'scope': 'control recurrence in current PERSPUV; not an RTL proof'}

def signed(value: int, width: int) -> int:
    value &= (1 << width) - 1
    return value - (1 << width) if value >> (width - 1) else value

def perspective(num: int, mant: int, k: int, width: int = 64):
    # Preserve six-bit shift arithmetic, width-limited two's-complement addition.
    sh = (32 - k) & 63
    prod = signed(signed(num, 32) * (mant & 0xFFFFFF), width)
    rounded = signed(prod + (1 << ((sh - 1) & 63)), width)
    resc = rounded >> sh
    sat = not (-(1 << 31) <= resc <= (1 << 31) - 1)
    result = max(-(1 << 31), min((1 << 31) - 1, resc))
    return result, sat

def arithmetic_domain():
    nums = [-(1 << 31), -(1 << 31)+1, -65537, -1, 0, 1, 65537, (1 << 31)-1]
    mants = [0, 1, 63, 64, 0x7FFFFF, 0x800000, 0xFFFFFE, 0xFFFFFF]
    comparisons = 0
    for num, mant, k in itertools.product(nums, mants, range(1, 25)):
        assert perspective(num, mant, k, 64) == perspective(num, mant, k, 56)
        comparisons += 1
    rng = random.Random(0x56)
    for _ in range(100000):
        num = signed(rng.getrandbits(32), 32)
        mant = rng.getrandbits(24); k = rng.randrange(1, 25)
        assert perspective(num, mant, k, 64) == perspective(num, mant, k, 56)
        comparisons += 1
    original = perspective(0, 1, 32, 64)
    narrowed = perspective(0, 1, 32, 56)
    assert original != narrowed
    RESULTS['P1_narrowing_domain'] = {'result': 'PASS', 'legal_domain_comparisons': comparisons,
        'counterexample_k32': {'original64': original, 'narrowed56': narrowed},
        'decision': 'keep 64-bit arithmetic in first architecture variant; narrowing is separately conditional'}

def paired_pipeline_credit():
    # Fixed-latency arithmetic, one ordered terminal FIFO. Capacity counts ALL
    # pending records, not only FIFO residents. No same-edge full credit bypass.
    cap, latency = 17, 6
    total_in = total_out = resets = max_owned = stalls = cancelled = 0
    products = 0
    for seed in range(24):
        rng = random.Random(0xA710 + seed)
        pipe = deque([None]*latency)
        fifo = deque(); expected = deque(); owned = 0; pending = None
        tag = 0
        for cyc in range(6000):
            if rng.randrange(1500) == 0:
                cancelled += len(expected)
                pipe = deque([None]*latency); fifo.clear(); expected.clear()
                owned = 0; pending = None; resets += 1
                continue
            if pending is None and rng.random() < .96:
                u, v = signed(rng.getrandbits(32), 32), signed(rng.getrandbits(32), 32)
                mant, k = rng.getrandbits(24), rng.randrange(64)
                zero = rng.randrange(5) == 0
                uq, us = perspective(u, mant, k); vq, vs = perspective(v, mant, k)
                pending = (tag, 0 if zero else uq, 0 if zero else vq,
                           False if zero else (us or vs), zero)
                tag += 1
            ready = (rng.random() < .73) and not (80 <= cyc % 509 < 180)
            pop = bool(fifo) and ready
            acc = pending is not None and owned < cap
            if pop:
                out = fifo.popleft()
                assert expected and expected.popleft() == out
                total_out += 1
            terminal = pipe.popleft()
            if terminal is not None:
                assert len(fifo) < cap
                fifo.append(terminal)
            pipe.append(pending if acc else None)
            if acc:
                expected.append(pending)
                products += 0 if pending[4] else 2
                pending = None; total_in += 1
            owned += int(acc)-int(pop)
            assert 0 <= owned <= cap
            assert owned == len(fifo) + sum(p is not None for p in pipe)
            assert owned == len(expected)
            max_owned = max(max_owned, owned)
            stalls += int(pending is not None and not acc)
        for _ in range(latency+cap+5):
            if fifo:
                assert expected.popleft() == fifo.popleft()
                owned -= 1; total_out += 1
            terminal = pipe.popleft(); pipe.append(None)
            if terminal is not None:
                fifo.append(terminal)
        assert owned == 0 and not expected and not fifo
    # Saturation in the abstract model: uninterrupted acceptance and retirement.
    pipe = deque([None]*latency); fifo = deque(); owned = 0
    accepts, emits = [], []
    for cyc in range(150):
        pop = bool(fifo); acc = owned < cap
        if pop:
            fifo.popleft(); emits.append(cyc)
        terminal = pipe.popleft(); pipe.append(cyc if acc else None)
        if terminal is not None: fifo.append(terminal)
        if acc: accepts.append(cyc)
        owned += int(acc)-int(pop)
    assert accepts == list(range(150))
    assert all(b-a == 1 for a,b in zip(emits, emits[1:]))
    assert total_in == total_out + cancelled
    RESULTS['P2_paired_pipeline_credit'] = {'result': 'PASS', 'seeds': 24,
        'cycles_per_seed': 6000, 'credit_capacity_including_pipeline': cap,
        'max_owned': max_owned, 'accepted': total_in, 'emitted': total_out,
        'reset_events': resets, 'cancelled_records': cancelled, 'input_stall_cycles': stalls,
        'nonzero_product_jobs': products, 'saturation_pair_interval_clocks': 1,
        'scope': 'packet/credit model, not DSP RTL, FIFO RAM implementation, or timing proof'}

def owner_local_boolean():
    cases = 0
    for target_req, target_iss, mask_index, identity_ok in itertools.product(
            range(16), range(16), range(4), (False, True)):
        mask = 1 << mask_index
        global_ok = identity_ok and bool(target_req & mask) and not bool(target_iss & mask)
        for is_target in (False, True):
            # For nontarget, local row contents are immaterial: target term kills update.
            local_ok = is_target and identity_ok and bool(target_req & mask) and not bool(target_iss & mask)
            assert (global_ok and is_target) == local_ok
            cases += 1
    RESULTS['O0_issue_predicate_localization'] = {'result': 'PASS', 'cases': cases,
        'scope': 'Boolean update identity only; no claim about fitted sharing, reject counters, or full-owner equivalence'}

def geometry():
    original = [64,32,8,8,2,1,2,8,2,3,8]
    proposed = [('ctx',64),('lod',8),('raw_class',2),('aux',1),('count',2),
                ('pal_slot',2),('pal_gen',8),('mosaic_a',8),('mosaic_b',8),
                ('mosaic_weight',8),('binding',8)]
    assert sum(original) == 138 and sum(w for _,w in proposed) == 119
    assert (21*64*3) == 4032
    assert 256*40 == 10240
    widths = [40,46,64,80,82,111,119,138]
    RESULTS['G0_declared_geometry'] = {'result': 'PASS',
        'original_front_array_declared_bits': sum(original)*64,
        'proposed_front_descriptor_bits_per_row': 119,
        'old_sample_reference_declared_bits': 4032,
        'parallel_SDP_M10K_width_lower_bounds': {str(w):(w+39)//40 for w in widths},
        'warning': 'declared bits and geometric lower bounds are not measured flip-flops, ALMs, or inference results'}

def main():
    for check in (stalled_join_counterexample, repaired_join_random, axis_queue_induction,
                  arithmetic_domain, paired_pipeline_credit, owner_local_boolean, geometry):
        check()
        print('PASS', check.__name__)
    path = Path(__file__).with_name('independent_results.json')
    path.write_text(json.dumps(RESULTS, indent=2)+'\n', encoding='utf-8')
    print('7/7 check groups passed. See independent_results.json for exact scope and traces.')
    print('NOT run: Verilator/RTL simulation, formal RTL proof, Quartus, board tests.')

if __name__ == '__main__':
    main()
