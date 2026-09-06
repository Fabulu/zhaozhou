import io,re,sys
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

def keycount(t,brace):
    """count top-level {f,v} pairs"""
    db=dp=0; items=0; j=brace; ne=False
    while j<len(t):
        c=t[j]
        if c=='{': db+=1
        elif c=='}':
            db-=1
            if db==0: break
        elif c=='(': dp+=1
        elif c==')': dp-=1
        elif c==',' and db==1 and dp==0: items+=1
        elif db>=1 and not c.isspace(): ne=True
        j+=1
    if not ne: return 0
    body=t[brace+1:j].rstrip()
    return items if body.endswith(',') else items+1

for path in sys.argv[1:]:
    raw=io.open(path,encoding='utf-8',errors='replace').read()
    t=scrub(raw)
    # all Key-array declarations, in order
    decls=[]
    for m in re.finditer(r'\b(?:static\s+)?(?:const|constexpr)\s+Key\s+(\w+)\s*\[\s*\]\s*=\s*\{',t):
        nm=m.group(1); n=keycount(t,m.end()-1)
        decls.append((m.start(), nm, n, t[:m.start()].count('\n')+1))
    print('=== %s : %d Key tables ==='%(path.split('/')[-1],len(decls)))
    bad=0
    for m in re.finditer(r'\bcurve(?:_half)?\s*\(\s*(\w+)\s*,\s*(\d+)\s*,',t):
        nm,passed=m.group(1),int(m.group(2))
        cands=[d for d in decls if d[1]==nm and d[0]<m.start()]
        if not cands:
            print('  ?? %s: no preceding decl'%nm); continue
        pos,_,real,dl=cands[-1]
        ln=t[:m.start()].count('\n')+1
        mark='' if passed==real else ('  <<<< MISMATCH real=%d passed=%d'%(real,passed))
        if mark: bad+=1
        print('  %s:%d  curve(%s, %d)   decl@%d has %d keys%s'%(path.split('/')[-1],ln,nm,passed,dl,real,mark))
    print('  MISMATCHES: %d\n'%bad)
