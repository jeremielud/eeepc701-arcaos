import sys,re
pk={};prov={}
cur={}
for line in open('idx/APKINDEX',encoding='utf-8'):
    line=line.rstrip('\n')
    if not line:
        if cur: pk[cur['P']]=cur
        for p in cur.get('p',''): prov.setdefault(re.split('[<>=~]',p)[0],cur['P'])
        cur={};continue
    k,v=line[0],line[2:]
    if k in 'pD': cur[k]=v.split()
    else: cur[k]=v
for n,c in pk.items(): prov.setdefault(n,n)
want=sys.argv[1:]; seen=set(); stack=list(want)
while stack:
    d=stack.pop()
    if d.startswith('!'): continue
    name=re.split('[<>=~]',d)[0]
    p=prov.get(name)
    if p is None: print('MANQUE',d,file=sys.stderr); continue
    if p in seen: continue
    seen.add(p); stack+= pk[p].get('D',[])
for p in sorted(seen): print(f"{p}-{pk[p]['V']}.apk")
