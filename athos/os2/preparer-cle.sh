#!/bin/sh
# preparer-cle.sh : copie le pilote et ses outils dans \ATHOS sur la clé USB du 701
# (démarré sur la clé), dans sa partition FAT16 « ATHOS », prêts pour ATHOSINS.CMD dans ArcaOS.
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
# Partition FAT16 « ATHOS » (2e partition de la clé) : ArcaOS ne lit pas le FAT32
# de la première partition sans le module FAT32.
ssh eeepc '[ -b /dev/sdb2 ] || mknod /dev/sdb2 b 8 18; mkdir -p /mnt/athos; if mountpoint -q /mnt/athos; then mount -o remount,rw /mnt/athos; else mount -t vfat /dev/sdb2 /mnt/athos; fi; mkdir -p /mnt/athos/ATHOS'
scp -q $T/* eeepc:/mnt/athos/ATHOS/
ssh eeepc 'sync; ls -la /mnt/athos/ATHOS; umount /mnt/athos'
rm -rf $T
