import io,re,os,subprocess,sys
sh=io.open("tools/sweep_geom_lod.sh",encoding="utf-8",newline="").read()
rtl_path="fpga/rtl/geometry/zhao_geom_lod.sv"
gold=io.open(rtl_path,encoding="utf-8",newline="").read()
body=sh[sh.index("MUTS=("):sh.index("\nexpected=")]
ents=re.findall(r'^"(.*?)"$', body, re.M|re.S)
vr=os.environ["VERILATOR_ROOT"]
bad=[]
for e in ents:
    name,old,new=e.split("@@")
    NL="\r\n" if "\r\n" in gold else "\n"
    o=old.replace("\n",NL); nw=new.replace("\n",NL)
    if gold.count(o)!=1:
        bad.append((name.strip(),"anchor x%d"%gold.count(o))); continue
    io.open(rtl_path,"w",encoding="utf-8",newline="").write(gold.replace(o,nw,1))
    rc=subprocess.run([vr+"/bin/verilator_bin.exe","--lint-only","-Wall","-Wno-DECLFILENAME",rtl_path],
                      capture_output=True,text=True)
    if rc.returncode!=0:
        first=[l for l in (rc.stdout+rc.stderr).splitlines() if "%Error" in l or "%Warning" in l]
        bad.append((name.strip(), first[0][:90] if first else "rc=%d"%rc.returncode))
io.open(rtl_path,"w",encoding="utf-8",newline="").write(gold)
print("linted %d mutants, %d do not build" % (len(ents),len(bad)))
for n,why in bad: print("   %-42s %s" % (n,why))
sys.exit(1 if bad else 0)
