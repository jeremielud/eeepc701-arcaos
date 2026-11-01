#!/bin/sh
# construire.sh : compile le pilote OS/2 athos.os2 depuis Linux avec OpenWatcom.
# Il faut, hors du dépôt (non redistribuables) : OpenWatcom dans ../../watcom, le kit
# Drv32 et le MiniDDK de David Azarewicz (https://88watts.net/software.html) dans
# ../../telechargements/drv32 et ../../telechargements/miniddk.
set -e
cd "$(dirname "$0")"
R="$(cd ../.. && pwd)"
export WATCOM="$R/watcom"
export PATH="$WATCOM/binl64:$WATCOM/binl:$PATH"
KIT="$R/telechargements/drv32/Kit"
DDK="$R/telechargements/miniddk"
export INCLUDE="$KIT:$WATCOM/h:$DDK/base/h:$DDK/base32/h:$DDK/base/inc"
O=obj
mkdir -p $O
# Options du kit pour un pilote 32 bits (Required/makefile, C32DRVFLAGS), en C99.
DRV="-q -bt=os2 -6s -obilmr -s -ze -zl -zq -zfp -zgp -ms -zp4 -za99 -DATHOS_NRX=16 -DATHOS_NTX=8"
for f in attach caps desc dma eeprom gpio initvals pcu phy qcu reset glue athos wlan; do
	wcc386 $DRV -I../core -I../compat -I.. -fo=$O/$f.obj ../core/$f.c
done
wcc386 $DRV -I../core -I../compat -fo=$O/osdep_os2.obj osdep_os2.c
wcc386 $DRV -I../core -I../compat -fo=$O/pilote.obj pilote.c
# Le module d'en-tête du pilote, extrait de la bibliothèque du kit.
T=$(mktemp -d)
( cd $T && wlib -q -x "$KIT/Drv32.lib" )
mv "$T/F:\src\os2\Drv32\header.o" $O/header.obj
rm -rf "$T"
cat > $O/athos.lrf <<FIN
name athos.os2
format os2 lx phys
option mixed1632,nostub,align=4
option quiet,map=$O/athos.map,caseexact,eliminate
option stack=0
option description '@#jeremie:0.1#@ athos : pilote Wi-Fi AR2425 pour OS/2 (essai)'
segment _TEXT PRELOAD CONFORMING IOPL DYNAMIC
segment TYPE DATA SHARED
file $O/header.obj
file $O/pilote.obj
file $O/osdep_os2.obj
FIN
for f in attach caps desc dma eeprom gpio initvals pcu phy qcu reset glue athos wlan; do
	echo "file $O/$f.obj" >> $O/athos.lrf
done
cat >> $O/athos.lrf <<FIN
library $KIT/Drv32.lib
library $DDK/base32/lib/kee.lib
library $DDK/base/lib/os2386p.lib
FIN
wlink @$O/athos.lrf
ls -l athos.os2
