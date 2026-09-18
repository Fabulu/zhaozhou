"""Generate reproducible vectors and run optional RTL candidates with Icarus.
Returns 2 when tools are unavailable. No skipped test is labelled a pass.
"""
from pathlib import Path
import argparse
import random
import shutil
import subprocess
import sys
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'implementation'))
from closure_support import upload_verdict


def vectors():
    rng=random.Random(20260918)
    cases=[]
    for i in range(4096):
        source_base=(rng.randrange(1024)+1)*4096
        dest_base=(rng.randrange(1024)+1)*4096
        source_bytes=dest_bytes=4096
        length=64*rng.randrange(1,65)
        hps=source_base;dst=dest_base
        re=ce=rng.randrange(65536)
        mode=i%12
        if mode==1:length=0;hps+=1
        elif mode==2:hps+=1
        elif mode==3:dst+=1
        elif mode==4:length+=1
        elif mode==5:hps+=(1<<32)
        elif mode==6:hps=source_base-64
        elif mode==7:hps=source_base+4096
        elif mode==8:dst=dest_base-64
        elif mode==9:dst=dest_base+4096
        elif mode==10:ce=(ce+1)&65535
        vals=(hps,dst,length,source_base,source_bytes,dest_base,dest_bytes,re,ce)
        cases.append((vals,int(upload_verdict(*vals))))
    for length in (64,128):
        vals=(0xffffffc0,8192,length,0xffffff80,128,8192,1024,3,3)
        cases.append((vals,int(upload_verdict(*vals))))
    return cases


def main():
    p=argparse.ArgumentParser()
    p.add_argument('--generate-only',action='store_true')
    args=p.parse_args()
    out=root/'evidence'/'rtl-work';out.mkdir(parents=True,exist_ok=True)
    rows=vectors()
    text=''.join(' '.join(f'{x:x}' for x in vals)+f' {code}\n' for vals,code in rows)
    (out/'upload_vectors.txt').write_text(text,encoding='ascii')
    print(f'Generated {len(rows)} deterministic vectors; vector generation is not RTL verification.')
    if args.generate_only:return 0
    iv=shutil.which('iverilog');vvp=shutil.which('vvp')
    if not iv or not vvp:
        print('NOT RUN: iverilog and vvp are required. RTL remains unverified.',file=sys.stderr)
        return 2
    target=out/'candidates.vvp'
    sources=[root/'implementation'/'zhao_cpl_ram_fifo.sv',root/'implementation'/'zhao_cpl_upload_guard.sv',root/'tests'/'rtl_candidate_tb.sv']
    subprocess.run([iv,'-g2012','-s','rtl_candidate_tb','-o',str(target),*(str(f) for f in sources)],check=True)
    subprocess.run([vvp,str(target)],cwd=out,check=True)
    return 0
if __name__=='__main__':
    try:sys.exit(main())
    except subprocess.CalledProcessError as exc:sys.exit(exc.returncode or 1)
