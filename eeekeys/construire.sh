#!/bin/sh
# construire.sh : compile eeekeys.exe (programme OS/2) avec OpenWatcom depuis Linux.
# Il faut OpenWatcom dans ../watcom et le kit ACPI d'Arca Noae dans
# ../telechargements/acpitk (https://88watts.net/download/acpi-toolkit-3.23.zip).
# Le source est en UTF-8 ; il est compilé en page de code 850, celle d'OS/2.
set -e
cd "$(dirname "$0")"
WATCOM="$(cd ../watcom && pwd)"
TK="$(cd ../telechargements/acpitk && pwd)"
export WATCOM PATH="$WATCOM/binl64:$WATCOM/binl:$PATH"
export INCLUDE="$WATCOM/h:$WATCOM/h/os2:$TK/h:$TK/h/platform"
iconv -f utf-8 -t cp850 eeekeys.c > eeekeys-850.c
wcc386 -bt=OS2 -bm -6s -fpi -zq -zp4 -w1 -DACPI_APPLICATION -fo=eeekeys.o eeekeys-850.c
wlink system os2v2_pm name eeekeys.exe file eeekeys.o library "$TK/lib/acpi32.lib" option quiet
rm -f eeekeys.o eeekeys-850.c
ls -l eeekeys.exe
