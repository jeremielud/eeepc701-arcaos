#!/usr/bin/env python3
"""souris.py X Y [clic|double|droit] : amène le pointeur en (X, Y) puis clique.
Position courante (pointe de la flèche) dans souris.pos ; recalée par différence d'images."""
import sys, subprocess, time, os
from PIL import Image, ImageChops
os.chdir(os.path.dirname(os.path.abspath(__file__)))
def mon(cmd):
    subprocess.run(['socat','-','UNIX-CONNECT:mon.sock'],input=(cmd+'\n').encode(),capture_output=True)
def shot(name):
    mon(f'screendump {os.getcwd()}/ecrans/{name}.png -f png'); time.sleep(0.4)
    return Image.open(f'ecrans/{name}.png').convert('RGB')
x,y=map(int,open('souris.pos').read().split())
tx,ty=int(sys.argv[1]),int(sys.argv[2])
for _ in range(6):
    dx,dy=tx-x,ty-y
    if abs(dx)<=2 and abs(dy)<=2: break
    before=shot('_av')
    # pas de 5 unités ≈ 1,76 px par unité
    ux,uy=round(dx/1.76),round(dy/1.76)
    while ux or uy:
        sx=max(-5,min(5,ux)); sy=max(-5,min(5,uy))
        mon(f'mouse_move {sx} {sy}'); ux-=sx; uy-=sy; time.sleep(0.05)
    time.sleep(0.6)
    after=shot('_ap')
    # nouvelle position : parmi les amas de pixels changés, celui le plus proche de
    # la cible ; la pointe de la flèche est le coin haut-gauche de son cadre.
    diff=ImageChops.difference(before,after).convert('L').point(lambda v:255 if v>40 else 0)
    px=diff.load(); W,H=diff.size
    seen=set(); best=None
    for yy in range(0,H,2):
        for xx in range(0,W,2):
            if px[xx,yy] and (xx,yy) not in seen:
                stack=[(xx,yy)]; seen.add((xx,yy)); x0=x1=xx; y0=y1=yy
                while stack:
                    a,b=stack.pop()
                    x0=min(x0,a);x1=max(x1,a);y0=min(y0,b);y1=max(y1,b)
                    for c,d in((a+2,b),(a-2,b),(a,b+2),(a,b-2)):
                        if 0<=c<W and 0<=d<H and px[c,d] and (c,d) not in seen:
                            seen.add((c,d)); stack.append((c,d))
                if x1-x0>40 or y1-y0>40: continue
                dd=abs(x0-tx)+abs(y0-ty)
                if best is None or dd<best[0]: best=(dd,y0,x0)
    if best: y,x=best[1],best[2]
    else: x,y=tx,ty
open('souris.pos','w').write(f'{x} {y}')
act=sys.argv[3] if len(sys.argv)>3 else ''
if act in('clic','double'):
    for _ in range(2 if act=='double' else 1):
        mon('mouse_button 1'); time.sleep(0.15); mon('mouse_button 0'); time.sleep(0.15)
elif act=='droit':
    mon('mouse_button 2'); time.sleep(0.15); mon('mouse_button 0')
print(x,y)
