#!/usr/bin/env python3
"""tape-fr.py "texte" [ret] : tape du texte quand OS/2 est en clavier français (AZERTY)."""
import sys, subprocess, time, os
os.chdir(os.path.dirname(os.path.abspath(__file__)))
swap={'a':'q','q':'a','z':'w','w':'z','m':'semicolon'}
keys={' ':'spc',':':'dot','.':'shift-comma','\\':'ralt-8','/':'shift-dot','-':'6','_':'8',
      '>':'shift-less','<':'less','&':'1','"':'3',"'":'4','\n':'ret'}
digits={str(d):'shift-'+str(d) for d in range(10)}
out=[]
for c in sys.argv[1]:
    if c.isalpha():
        k=swap.get(c.lower(),c.lower()); k=('shift-'+k) if c.isupper() else k
    elif c in digits: k=digits[c]
    elif c in keys: k=keys[c]
    else: raise SystemExit('caractère inconnu %r'%c)
    out.append(k)
if len(sys.argv)>2: out.append('ret')
for k in out:
    subprocess.run(['socat','-','UNIX-CONNECT:mon.sock'],input=f'sendkey {k} 60\n'.encode(),capture_output=True)
    time.sleep(0.18)
