#!/bin/sh
# cycle.sh [--pilote] [commandes.cmd] : un aller-retour complet sur le 701, sans aide.
#   --pilote       compile et dépose d'abord la nouvelle version du pilote sur la clé ;
#   commandes.cmd  fichier de commandes OS/2 exécuté une fois dans ArcaOS (FAIRE.CMD).
# Le 701 (démarré sur la clé, Linux) redémarre une fois sur ArcaOS (chargeur de la
# clé, entrée « arcaos ») ; l'agent ATHOSAGT.CMD y relève l'état, exécute les
# commandes, installe le nouveau pilote et redémarre ; le 701 revient sur Linux et
# les résultats sont affichés (AGENT.TXT, ETAT.TXT, SORTIE.TXT).
set -e
cd "$(dirname "$0")"
if [ "$1" = --pilote ]; then
	./preparer-cle.sh > /dev/null
	shift
fi
ssh eeepc '[ -b /dev/sdb2 ] || mknod /dev/sdb2 b 8 18; mkdir -p /mnt/athos
	if mountpoint -q /mnt/athos; then mount -o remount,rw /mnt/athos; else mount -t vfat /dev/sdb2 /mnt/athos; fi
	cd /mnt/athos/ATHOS && rm -f AGENT.TXT ETAT.TXT SORTIE.TXT FAIT.CMD && : > RETOUR'
if [ -n "$1" ]; then
	iconv -f utf-8 -t cp850//TRANSLIT "$1" | sed 's/\r*$/\r/' | ssh eeepc 'cat > /mnt/athos/ATHOS/FAIRE.CMD'
fi
ssh eeepc 'sync; umount /mnt/athos
	command -v extlinux >/dev/null || apk add -q syslinux >/dev/null 2>&1
	M=$(awk "\$1==\"/dev/sdb1\"{print \$2}" /proc/mounts)
	mount -o remount,rw $M && extlinux --once=arcaos $M/boot/syslinux >/dev/null && sync && mount -o remount,ro $M
	(sleep 2; reboot) >/dev/null 2>&1 &'
echo "$(date +%T) : le 701 démarre ArcaOS"
sleep 90
debut=$(date +%s)
until ssh -o ConnectTimeout=5 eeepc true 2>/dev/null; do
	if [ $(( $(date +%s) - debut )) -gt 900 ]; then
		echo "le 701 n'est pas revenu sur Linux après 15 minutes"
		exit 1
	fi
	sleep 15
done
echo "$(date +%T) : de retour sur Linux"
ssh eeepc '[ -b /dev/sdb2 ] || mknod /dev/sdb2 b 8 18; mkdir -p /mnt/athos
	mountpoint -q /mnt/athos || mount -t vfat -o ro /dev/sdb2 /mnt/athos
	cd /mnt/athos/ATHOS
	for f in AGENT.TXT ETAT.TXT SORTIE.TXT; do [ -f $f ] && { echo "=== $f"; cat $f; }; done' | iconv -f cp850 -t utf-8
