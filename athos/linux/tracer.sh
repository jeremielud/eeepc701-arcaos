#!/bin/sh
# tracer.sh : SUR LE 701. Enregistre (mmiotrace) tous les accès du pilote ath5k à la
# puce pendant son initialisation et sa connexion au Wi-Fi, dans /root/athos/mmio.txt.
cd /root/athos
PCI=0000:01:00.0
T=/sys/kernel/tracing
WPA=/etc/wpa_supplicant/wpa_supplicant.conf
{
	echo "=== $(date) : trace mmiotrace"
	( sleep 180; echo "délai dépassé : redémarrage" >> essai.log; reboot -f ) &
	garde=$!
	mountpoint -q $T || mount -t tracefs nodev $T
	killall udhcpc wpa_supplicant 2>/dev/null
	sleep 1
	echo $PCI > /sys/bus/pci/drivers/ath5k/unbind
	echo 32768 > $T/buffer_size_kb
	echo mmiotrace > $T/current_tracer
	cat $T/trace_pipe > mmio.txt &
	lecteur=$!
	sleep 1
	echo $PCI > /sys/bus/pci/drivers/ath5k/bind
	sleep 3
	ip link set wlan0 up
	wpa_supplicant -B -i wlan0 -c $WPA
	i=0
	until wpa_cli -i wlan0 status 2>/dev/null | grep -q wpa_state=COMPLETED || [ $i -ge 30 ]; do sleep 1; i=$((i+1)); done
	sleep 3
	echo nop > $T/current_tracer
	sleep 1
	kill $lecteur
	echo "trace : $(wc -l < mmio.txt) lignes"
	udhcpc -i wlan0 -b -q
	sleep 5
	kill $garde
	echo "=== réseau retrouvé"
} > essai.log 2>&1
