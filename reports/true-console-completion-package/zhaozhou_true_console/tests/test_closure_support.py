import copy
import json
import random
import sys
import tempfile
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'implementation'))
from closure_support import *


def payload(age=0):
    d={name:0 for name,_,_,_ in LAYOUT}
    d.update(age=age,size=16)
    return pack_particle(d)


def record(i): return ParticleRecord(i, payload())


class CodecTests(unittest.TestCase):
    def test_layout_covers_exactly_128_bits(self):
        used=[]
        for _,start,bits,_ in LAYOUT: used.extend(range(start,start+bits))
        self.assertEqual(sorted(used),list(range(128)))
    def test_zero_and_unit_radius(self):
        self.assertEqual(unpack_particle(payload())['size'],16)
        self.assertEqual(len(payload()),16)
    def test_random_roundtrips(self):
        rng=random.Random(18092026)
        for _ in range(10000):
            d={}
            for name,_,w,signed in LAYOUT:
                d[name]=rng.randrange(-(1<<(w-1)),1<<(w-1)) if signed else rng.randrange(1<<w)
            d['flags'] &= 7
            self.assertEqual(unpack_particle(pack_particle(d)),d)
    def test_extremes(self):
        for high in (False, True):
            d={n: ((1<<(w-1))-1 if high else -(1<<(w-1))) if s else ((1<<w)-1 if high else 0) for n,_,w,s in LAYOUT}
            d['flags'] &= 7
            self.assertEqual(unpack_particle(pack_particle(d)),d)
    def test_reserved_and_overflow_refused(self):
        d=unpack_particle(payload());d['flags']=8
        with self.assertRaises(ValueError): pack_particle(d)
        d['flags']=0;d['vx']=1024
        with self.assertRaises(ValueError): pack_particle(d)
        with self.assertRaises(ValueError): unpack_particle(b'\0'*15)


class ParticleAdmissionTests(unittest.TestCase):
    def test_survivors_precede_children(self):
        r=finish_particle_tick([record(1),record(2)],[SpawnGroup(0,0,(record(3),record(4)))],4)
        self.assertEqual([p.stable_id for p in r.records],[1,2,3,4])
        self.assertEqual((r.children_requested,r.children_emitted,r.children_refused),(2,2,0))
    def test_whole_group_capacity_and_strict_prefix(self):
        r=finish_particle_tick([record(1)],[SpawnGroup(0,0,(record(2),record(3))),SpawnGroup(1,0,(record(4),))],2)
        self.assertEqual([p.stable_id for p in r.records],[1])
        self.assertEqual(r.children_refused,3)
    def test_16_valid_17_refused(self):
        groups=[SpawnGroup(0,0,tuple(record(i) for i in range(1,17))), SpawnGroup(1,0,tuple(record(i) for i in range(17,34)))]
        r=finish_particle_tick([],groups,100)
        self.assertEqual(len(r.records),16)
        self.assertEqual(r.malformed_groups,1)
    def test_order_violation_refused(self):
        with self.assertRaises(ValueError): finish_particle_tick([],[SpawnGroup(2,0,()),SpawnGroup(1,0,())],9)
    def test_survivors_cannot_be_evicted(self):
        with self.assertRaises(ValueError): finish_particle_tick([record(1),record(2)],[],1)
    def test_replay_and_population_tier(self):
        ss=[record(i) for i in range(32768)]
        groups=[SpawnGroup(i,0,(record(32768+i),)) for i in range(64)]
        a=finish_particle_tick(ss,groups,32768);b=finish_particle_tick(ss,groups,32768)
        self.assertEqual(a,b);self.assertEqual(a.children_refused,64)
        self.assertEqual([p.stable_id for p in a.records],list(range(32768)))


class UploadTests(unittest.TestCase):
    def test_verdict_order_and_boundaries(self):
        def v(**kw):
            d=dict(hps_addr=4096,vram_addr=8192,length=64,source_base=4096,source_bytes=1024,dest_base=8192,dest_bytes=1024,request_epoch=3,current_epoch=3)
            d.update(kw);return upload_verdict(**d)
        self.assertEqual(v(),UploadVerdict.OK)
        self.assertEqual(v(length=0,hps_addr=1),UploadVerdict.ZERO_LENGTH)
        self.assertEqual(v(hps_addr=(1<<32)+1),UploadVerdict.UNALIGNED)
        self.assertEqual(v(hps_addr=(1<<32)),UploadVerdict.SOURCE_UNREACHABLE)
        self.assertEqual(v(hps_addr=0),UploadVerdict.SOURCE_OUTSIDE_ARENA)
        self.assertEqual(v(vram_addr=0),UploadVerdict.OUTSIDE_GUARD)
        self.assertEqual(v(current_epoch=4),UploadVerdict.EPOCH_STALE)
        self.assertEqual(v(length=1024),UploadVerdict.OK)
        self.assertEqual(v(length=1088),UploadVerdict.SOURCE_OUTSIDE_ARENA)
        self.assertEqual(v(hps_addr=0xffffffc0,source_base=0xffffff80,source_bytes=128),UploadVerdict.OK)
        self.assertEqual(v(hps_addr=0xffffffc0,source_base=0xffffff80,source_bytes=128,length=128),UploadVerdict.SOURCE_OUTSIDE_ARENA)
    def txn(self): return UploadTransaction(77,8,3,4,9)
    def test_waits_for_retirement(self):
        t=self.txn();t.issue(8);t.close_issue();t.complete_crc(True)
        with self.assertRaises(ValueError):t.publish(3,True,(8,3))
        t.retire(77,0,7)
        with self.assertRaises(ValueError):t.publish(3,True,(8,3))
        t.retire(77,7)
        self.assertEqual(t.publish(3,True,(8,3)),(9,4))
        with self.assertRaises(ValueError):t.publish(3,True,(8,3))
    def test_fresh_slot_required(self):
        t=self.txn();t.issue(8);t.retire(77,0,8);t.close_issue();t.complete_crc(True)
        with self.assertRaises(ValueError):t.publish(3,True,(9,3))
    def test_abort_quarantines_until_drain(self):
        t=self.txn();t.issue(3);t.abort()
        self.assertFalse(t.reclaimable)
        t.retire(77,0,3);self.assertTrue(t.reclaimable)
        with self.assertRaises(ValueError):t.issue()
        with self.assertRaises(ValueError):t.publish(3,True,(8,3))
    def test_stale_epoch_blocks_publish(self):
        t=self.txn();t.issue(8);t.retire(77,0,8);t.close_issue();t.complete_crc(True)
        with self.assertRaises(ValueError):t.publish(4,True,(8,3))
        self.assertTrue(t.reclaimable)
    def test_duplicate_foreign_and_unissued_credit(self):
        t=self.txn();t.issue(2)
        self.assertFalse(t.retire(78,0))
        self.assertEqual(t.retired,0)
        t.retire(77,0)
        with self.assertRaises(ValueError):t.retire(77,0)
        with self.assertRaises(ValueError):t.retire(77,2)
    def test_random_retirement_permutations(self):
        rng=random.Random(91)
        for _ in range(200):
            t=UploadTransaction(7,32,1,2,3);t.issue(32);t.close_issue();t.complete_crc(True)
            order=list(range(32));rng.shuffle(order)
            for n in order[:-1]: t.retire(7,n)
            with self.assertRaises(ValueError):t.publish(1,True,(2,1))
            t.retire(7,order[-1]);self.assertEqual(t.publish(1,True,(2,1)),(3,2))


class PlanningTests(unittest.TestCase):
    def test_ram_width_and_ports(self):
        self.assertEqual(memory_plan(256,75)['blocks'],2)
        self.assertEqual(memory_plan(256,76)['blocks'],2)
        self.assertEqual(memory_plan(256,76,'tdp')['blocks'],4)
        self.assertEqual(memory_plan(16,256)['blocks'],7)
        self.assertEqual(memory_plan(256,32,copies=3)['blocks'],3)
    def test_nonempty_ram_validation(self):
        for d,w in [(0,3),(3,0),(-1,4)]:
            with self.assertRaises(ValueError):memory_plan(d,w)
    def test_provider_bounds_not_certificate(self):
        a=provider_bound(480000,147,1,1333333)
        self.assertTrue(a['impossible_even_without_stalls'])
        b=provider_bound(32768,8,1,1333333)
        self.assertFalse(b['impossible_even_without_stalls'])
        self.assertFalse(b['schedule_proven'])
    def ledger(self):
        return {'schema':1,'owners':{'a':{'kind':'rtl','fitted_alms':None}},'capabilities':[{'id':'X','policy':'required','state':'candidate','owner':'a','implementation':['x.sv'],'evidence':{},'notes':'candidate'}]}
    def test_unknown_scope_not_zero(self):
        d=self.ledger();self.assertTrue(check_ledger(d))
        with self.assertRaises(LedgerError):check_ledger(d,release=True)
    def test_no_speculative_area_as_fit(self):
        d=self.ledger();d['owners']['a'].update(fitted_alms=400,area_evidence='estimate')
        with self.assertRaises(LedgerError):check_ledger(d)
    def test_duplicate_capability_refused(self):
        d=self.ledger();d['capabilities'].append(copy.deepcopy(d['capabilities'][0]))
        with self.assertRaises(LedgerError):check_ledger(d)
    def test_file_existence_and_digest_changes(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);(root/'x.sv').write_text('module x; endmodule\n')
            a=snapshot_files(root,['x.sv']);(root/'x.sv').write_text('module x; wire a; endmodule\n')
            b=snapshot_files(root,['x.sv']);self.assertNotEqual(a['manifest_sha256'],b['manifest_sha256'])
            with self.assertRaises(ValueError):snapshot_files(root,['../missing'])


if __name__ == '__main__': unittest.main(verbosity=2)
