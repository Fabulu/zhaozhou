import io,os,re,sys
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
import importlib.util
spec=importlib.util.spec_from_file_location('h', os.path.join(os.path.dirname(os.path.abspath(__file__)),'hunt.py'))
# re-implement scrub/count locally to avoid running hunt's main
def scrub(t):
    out,i,n=[],0,len(t)
    while i<n:
        c=t[i]
        if c=='/' and i+1<n and t[i+1]=='/':
            j=t.find('\n',i); j=n if j<0 else j
            out.append(' '*(j-i)); i=j
        elif c=='/' and i+1<n and t[i+1]=='*':
            j=t.find('*/',i+2); j=n if j<0 else j+2
            out.append(''.join(ch if ch=='\n' else ' ' for ch in t[i:j])); i=j
        elif c in '"\'':
            q,j=c,i+1
            while j<n and t[j]!=q: j+=2 if t[j]==chr(92) else 1
            j=min(j+1,n)
            out.append(''.join(ch if ch=='\n' else 'S' for ch in t[i:j])); i=j
        else:
            out.append(c); i+=1
    return ''.join(out)
def count_entries(t,brace):
    db=dp=ds=0; items=0; j=brace; ne=False
    while j<len(t):
        c=t[j]
        if c=='{': db+=1
        elif c=='}':
            db-=1
            if db==0: break
        elif c=='(': dp+=1
        elif c==')': dp-=1
        elif c=='[': ds+=1
        elif c==']': ds-=1
        elif c==',' and db==1 and dp==0 and ds==0: items+=1
        elif db>=1 and not c.isspace(): ne=True
        j+=1
    if not ne: return 0
    body=t[brace+1:j].rstrip()
    return items if body.endswith(',') else items+1

R=sys.argv[1]
CARR=re.compile(r'\b(?:constexpr|const|static)\b[\w:<>,\s*&]*?\b(\w{2,})\s*\[\s*([\w+\-* ]*?)\s*\]\s*=\s*\{')
srcs={}
for f in sorted(os.listdir(R)):
    if f.endswith(('.h','.cpp')): srcs[f]=scrub(io.open(os.path.join(R,f),encoding='utf-8',errors='replace').read())
allsrc='\n'.join(srcs.values())
print('%-26s %-22s %5s %-10s  %s'%('FILE','TABLE','N','DECL_SIZE','indexing / bounds seen'))
for f,t in srcs.items():
    if not f.startswith('manafold'): continue
    for m in CARR.finditer(t):
        name,size=m.group(1),m.group(2).strip()
        n=count_entries(t,m.end()-1)
        if not n or n<2: continue
        lits=sorted({int(x) for x in re.findall(re.escape(name)+r'\s*\[\s*(\d+)\s*\]',allsrc)})
        var=len(re.findall(re.escape(name)+r'\s*\[\s*(?!\d+\s*\])',allsrc))
        sz=bool(re.search(r'sizeof\s*\(\s*'+re.escape(name)+r'\s*\)',allsrc))
        flag=''
        if size.isdigit() and int(size)!=n: flag+=' !!SIZE-MISMATCH'
        if lits and max(lits)<n-1 and not sz: flag+=' !!MAXLIT=%d<%d'%(max(lits),n-1)
        if not lits and not var: flag+=' !!NEVER-INDEXED'
        print('%-26s %-22s %5d %-10s  lits=%s var=%d sizeof=%s%s'%(f,name,n,size or '[]',lits[:8],var,sz,flag))
