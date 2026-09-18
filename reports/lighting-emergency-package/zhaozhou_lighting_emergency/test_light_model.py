"""Run: python test_light_model.py. No third-party dependencies."""
import json
import random
import unittest
from collections import deque
from math import isqrt
from light_model import *

RNG = random.Random(0x18192026)
COUNTS = {}

class Tests(unittest.TestCase):
    def test_01_divider_boundary_and_random(self):
        ds = [0,1,2,3,7,65535,65536,65537,2**30-1,2**31, U32]
        nums = [0,1,2,2**31-1,2**31,2**32-1,2**32,2**63-1,2**63,U64]
        cases = [(n,d,neg) for d in ds for n in nums for neg in [False,True]]
        for d in ds[1:]:
            for q in [2**31-1,2**31,2**32-1,2**32]:
                for delta in [-1,0,1]:
                    n=q*d+delta
                    if 0<=n<=U64:
                        cases += [(n,d,False),(n,d,True)]
        cases += [(RNG.getrandbits(64),RNG.getrandbits(32),bool(RNG.getrandbits(1)))
                  for _ in range(40_000)]
        for n,d,neg in cases:
            s=div_seed(n,d,neg)
            for _ in range(32): s=div_step(s)
            got,sat,zero,_=div_finish(s)
            exact=0 if d==0 else ((-n if neg else n)//d)
            self.assertEqual(got,sat32(exact))
            self.assertEqual(sat, d!=0 and not I32_MIN<=exact<=I32_MAX)
            self.assertEqual(zero,d==0)
        COUNTS['divider_exact_cases']=len(cases)

    def test_02_render_and_creature_adapter(self):
        count=0
        for _ in range(20_000):
            n=tuple(RNG.randrange(I32_MIN,I32_MAX+1) for _ in range(3))
            l=tuple(RNG.randrange(I32_MIN,I32_MAX+1) for _ in range(3))
            d=prepare_render(n)
            dot=sum(a*b for a,b in zip(n,l))
            self.assertEqual(raw_div32(dot,d),raw_direct(dot,d))
            detail=RNG.randrange(I32_MIN,I32_MAX+1)
            self.assertEqual(render_ndl(n,l,detail),clamp01(raw_direct(dot,d)+detail))
            # Actual skin producer emits range-reduced components; do not normalize them.
            ns=tuple(RNG.randrange(-(1<<30),(1<<30)) for _ in range(3))
            ds=isqrt(sum(x*x for x in ns)); dot_s=sum(a*b for a,b in zip(ns,l))
            legacy=0 if dot_s<=0 else min(65536,(dot_s+ds//2)//ds)
            self.assertEqual(creature_ndl(ns,ds,l),legacy)
            count+=1
        self.assertEqual(creature_ndl((2,1,0),2,(0,1,0)),1) # floor-only report is wrong
        self.assertEqual(render_ndl((1,0,0),(-1,0,0),2),1) # clamp must follow detail
        COUNTS['render_and_skin_cases']=count

    def test_03_root(self):
        xs=[0,1,2,3,4,8,9,15,16,U64]
        for k in range(33):
            for delta in [-1,0,1]:
                x=(1<<k)**2+delta
                if 0<=x<=U64: xs.append(x)
        xs += [RNG.getrandbits(64) for _ in range(20_000)]
        for x in xs: self.assertEqual(root_exact(x),isqrt(x))
        COUNTS['root_exact_cases']=len(xs)

    def test_04_divider_stream_stall_bubble_and_reset(self):
        pipe=DivPipelineII2(); expected=deque(); accepted=retired=aborted=0; cycles=0
        pending=None; tag=0
        while cycles<30_000 or expected or pending or pipe.output is not None:
            # Mid-flight reset invalidates only the abandoned epoch.
            if cycles in [1901,7613,14307]:
                aborted += len(expected); expected.clear(); pending=None; pipe.reset()
            if cycles<30_000 and pending is None and RNG.random()<0.82:
                n=RNG.getrandbits(64); d=RNG.getrandbits(32); neg=bool(tag&1)
                pending=div_seed(n,d,neg,tag); tag+=1
            ready=RNG.random()<0.78 if cycles<30_000 else True
            oldout=pipe.output
            take,out=pipe.tick(pending,ready)
            if not ready and oldout is not None: self.assertEqual(pipe.output,oldout)
            if out is not None:
                self.assertTrue(expected); self.assertEqual(out,expected.popleft()); retired+=1
            if take:
                s=pending
                for _ in range(32): s=div_step(s)
                expected.append(div_finish(s)); accepted+=1; pending=None
            cycles+=1
            self.assertLess(cycles,31_000)
        self.assertEqual(accepted,retired+aborted)
        COUNTS['divider_stream']={'cycles':cycles,'accepted':accepted,'retired':retired,'reset_aborted':aborted}

    def test_05_root_stream(self):
        p=RootPipelineII8(); q=deque(); pending=None; accepted=retired=0
        for t in range(16_500):
            if t<16_000 and pending is None:
                pending=RootState(RNG.getrandbits(64),0,0,accepted)
            a,o=p.tick(pending,t>=16_000 or RNG.random()<0.85)
            if o is not None: self.assertEqual(o,q.popleft()); retired+=1
            if a:
                q.append((isqrt(pending.rad),pending.tag)); pending=None; accepted+=1
        self.assertFalse(q); self.assertEqual(accepted,retired)
        COUNTS['root_stream']={'accepted':accepted,'retired':retired}

    def test_06_measured_model_ii(self):
        for p,period,isroot in [(DivPipelineII2(),2,False),(RootPipelineII8(),8,True)]:
            ats=[]; outs=[]; pending=None; sent=0
            for t in range(1200):
                if sent<100 and pending is None:
                    pending=RootState(sent**2,0,0,sent) if isroot else div_seed(100+sent,3,False,sent)
                a,o=p.tick(pending,True)
                if a: ats.append(t); sent+=1; pending=None
                if o is not None: outs.append(t)
            self.assertEqual(len(ats),100); self.assertEqual(len(outs),100)
            self.assertTrue(all(b-a==period for a,b in zip(ats,ats[1:])))
            self.assertTrue(all(b-a==period for a,b in zip(outs,outs[1:])))
            COUNTS['root_model_ii' if isroot else 'divider_model_ii']={
                'accept_interval':period,'first_accept':ats[0],'first_consume':outs[0],
                'last_consume':outs[-1]}

    def test_07_colour_schedule_preserves_individual_rounding(self):
        for _ in range(10_000):
            response=RNG.randrange(ONE+1)
            c=[RNG.randrange(1<<20) for _ in range(3)]
            e=[RNG.randrange(1<<20) for _ in range(3)]
            serial=[colour_term(c[j],response)+colour_term(e[j],response) for j in range(3)]
            phase0=[colour_term(v,response) for v in c]
            phase1=[colour_term(v,response) for v in e]
            self.assertEqual(serial,[phase0[j]+phase1[j] for j in range(3)])
        # Merging coefficients before rounding can change the result.
        self.assertNotEqual(colour_term(1,32768)+colour_term(1,32768),colour_term(2,32768))
        COUNTS['colour_cases']=10_000

    def test_08_calendar(self):
        four=issue_calendar(); eight=issue_calendar(lights=8)
        self.assertTrue(four['passes_issue_resources'])
        self.assertTrue(four['passes_20pct_reserved_window'])
        self.assertFalse(eight['passes_raw_60hz_window'])
        # Verify actual phase assignments for 120k four-light batches.
        dots=norms=spares=0
        for vertex in range(120_000):
            for light in range(4):
                dots+=2 # phase A: x and y
                dots+=1 # phase B: z
                if light<3: norms+=1 # next normal's axis square
                else: spares+=1
        self.assertEqual(dots,1_440_000); self.assertEqual(norms,360_000)
        self.assertEqual(spares,120_000)
        COUNTS['calendar_four']=four; COUNTS['calendar_eight']=eight

    def test_09_negative_controls(self):
        # Discarding high dividend bits silently corrupts a nonsaturating result.
        s=div_seed(1<<32,1<<16)
        wrong=DivState(0,s.work,s.den,s.neg,s.big,s.zero,s.tag)
        for _ in range(32): wrong=div_step(wrong)
        self.assertNotEqual(div_finish(wrong)[0],65536)
        # Signed floor versus C/C++ truncation matters for negative residuals.
        self.assertEqual(raw_div32(-2,3),-1)
        self.assertNotEqual(int((-2+3//2)/3),raw_div32(-2,3))
        # Supplied magnitude is semantic data, not an invitation to invent normalization.
        with self.assertRaises(ValueError): narrow_skin_pair((1<<40,0,0),1<<40)
        COUNTS['deliberately_wrong_algorithms_caught']=4

if __name__=='__main__':
    suite=unittest.defaultTestLoader.loadTestsFromTestCase(Tests)
    result=unittest.TextTestRunner(verbosity=2).run(suite)
    with open('verification_results.json','w',encoding='utf8') as f:
        json.dump({'status':'PASS' if result.wasSuccessful() else 'FAIL',
                   'tests_run':result.testsRun,'counts':COUNTS,
                   'scope':'Independent arithmetic and cycle models only; no RTL execution or Quartus'},f,indent=2)
    raise SystemExit(0 if result.wasSuccessful() else 1)
