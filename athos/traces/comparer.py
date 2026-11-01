#!/usr/bin/env python3
"""comparer.py : dernière valeur écrite dans chaque registre, ath5k (mmiotrace) contre
athos (trace maison) ; affiche les registres qui diffèrent ou que seul l'un écrit."""
import sys
def ath5k(p, base=0xfbef0000):
    d={}
    for l in open(p):
        f=l.split()
        if f and f[0]=='W' and len(f)>5:
            a=int(f[4],16)-base
            if 0<=a<0x10000: d[a]=int(f[5],16)
    return d
def athos(p):
    d={}
    for l in open(p):
        f=l.split()
        if f and f[0]=='M': break          # début de l'écoute : état à comparer
        if f and f[0]=='W': d[int(f[1],16)]=int(f[2],16)
    return d
k=ath5k('ath5k-mmio.txt'); m=athos('trace-athos.txt')
ign=set(range(0x8800,0x9800))  # cache de clés
for a in sorted(set(k)|set(m)):
    if a in ign: continue
    kv,mv=k.get(a),m.get(a)
    if kv!=mv:
        f=lambda v:'--------' if v is None else '%08x'%v
        print('%04x  ath5k %s  athos %s'%(a,f(kv),f(mv)))
