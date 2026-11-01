#!/usr/bin/env python3
"""tape.py "texte" [ret] : tape du texte (clavier US) dans la machine."""
import sys, subprocess, time, os
os.chdir(os.path.dirname(os.path.abspath(__file__)))
plain={' ':'spc','-':'minus','=':'equal','[':'bracket_left',']':'bracket_right',';':'semicolon',
"'":'apostrophe','`':'grave_accent','\\':'backslash',',':'comma','.':'dot','/':'slash','\t':'tab','\n':'ret'}
shifted={'!':'1','@':'2','#':'3','$':'4','%':'5','^':'6','&':'7','*':'8','(':'9',')':'0','_':'minus',
'+':'equal','{':'bracket_left','}':'bracket_right',':':'semicolon','"':'apostrophe','~':'grave_accent',
'|':'backslash','<':'comma','>':'dot','?':'slash'}
cmds=[]
for c in sys.argv[1]:
    if c.isalpha() and c.isupper(): k='shift-'+c.lower()
    elif c.isalnum(): k=c
    elif c in plain: k=plain[c]
    elif c in shifted: k='shift-'+(shifted[c] if shifted[c] in '0123456789' else plain.get(shifted[c],shifted[c]))
    else: raise SystemExit('caractère inconnu %r'%c)
    cmds.append(k)
if len(sys.argv)>2 and sys.argv[2]=='ret': cmds.append('ret')
for k in cmds:
    subprocess.run(['socat','-','UNIX-CONNECT:mon.sock'],input=f'sendkey {k} 60\n'.encode(),capture_output=True)
    time.sleep(0.18)
