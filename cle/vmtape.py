#!/usr/bin/env python3
"""vmtape.py "texte" : tape dans la VM de test (console Alpine en AZERTY)."""
import subprocess,time,sys
def k(x): subprocess.run(['socat','-','UNIX-CONNECT:test.sock'],input=f'sendkey {x} 60\n'.encode(),capture_output=True); time.sleep(0.1)
swap={'a':'q','q':'a','z':'w','w':'z','m':'semicolon'}
sp={' ':'spc','/':'shift-dot','-':'6','|':'ralt-6','\n':'ret','.':'shift-comma',':':'dot','>':'shift-less','_':'8','*':'backslash'}
for c in sys.argv[1]:
    if c in sp: k(sp[c])
    elif c.isdigit(): k('shift-'+c)
    elif c.isupper(): k('shift-'+swap.get(c.lower(),c.lower()))
    else: k(swap.get(c,c))
