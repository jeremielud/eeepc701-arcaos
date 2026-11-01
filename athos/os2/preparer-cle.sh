#!/bin/sh
# preparer-cle.sh : copie le pilote et ses outils dans \ATHOS sur la clé USB du 701
# (démarré sur la clé), prêts pour ATHOSINS.CMD dans ArcaOS.
set -e
cd "$(dirname "$0")"
./construire.sh > /dev/null
T=$(mktemp -d)
cp athos.os2 $T/ATHOS.OS2
cp athoslog.exe $T/ATHOSLOG.EXE
for f in install/ATHOS.NIF install/ATHOSINS.CMD install/ATHOSJRN.CMD; do
	iconv -f utf-8 -t cp850//TRANSLIT "$f" | sed 's/\r*$/\r/' > $T/$(basename $f)
done
printf 'Livebox-F550\r\n' > $T/SSID.TXT
ssh eeepc 'mount -o remount,rw /media/usb && mkdir -p /media/usb/athos'
scp -q $T/* eeepc:/media/usb/athos/
ssh eeepc 'sync; ls -la /media/usb/athos; mount -o remount,ro /media/usb'
rm -rf $T
