#!/bin/sh
# lancer.sh PROGRAMME [ARGUMENTS] : à exécuter SUR LE 701 (clé USB). Le Wi-Fi étant
# la seule liaison, l'essai tourne seul, détaché de la session SSH :
#   1. coupe wpa_supplicant et retire la carte au pilote ath5k ;
#   2. lance l'essai (90 s au plus), sortie dans /root/athos/essai.log ;
#   3. rend la carte à ath5k et reconnecte le Wi-Fi.
# Sécurité : si le 701 n'a pas retrouvé le réseau en 180 s, il redémarre (la clé
# se reconnecte d'elle-même au démarrage).
cd /root/athos
PCI=0000:01:00.0
WPA=/etc/wpa_supplicant/wpa_supplicant.conf
{
	echo "=== $(date) : $*"
	( sleep 180; echo "délai dépassé : redémarrage" >> essai.log; reboot -f ) &
	garde=$!
	killall udhcpc wpa_supplicant 2>/dev/null
	sleep 1
	[ -e /sys/bus/pci/drivers/ath5k/$PCI ] && echo $PCI > /sys/bus/pci/drivers/ath5k/unbind
	setpci -s $PCI COMMAND=0x0006           # mémoire + maître de bus
	timeout -s KILL 90 "$@"
	echo "code de sortie : $?"
	echo $PCI > /sys/bus/pci/drivers/ath5k/bind
	sleep 3
	ip link set wlan0 up
	wpa_supplicant -B -i wlan0 -c $WPA
	i=0
	until wpa_cli -i wlan0 status 2>/dev/null | grep -q wpa_state=COMPLETED || [ $i -ge 40 ]; do sleep 1; i=$((i+1)); done
	udhcpc -i wlan0 -b -q
	sleep 5
	kill $garde
	echo "=== réseau retrouvé"
} > essai.log 2>&1
