#!/bin/sh
# disque.sh : démarre la machine sur le disque, en émulation, sans carte réseau.
cd "$(dirname "$0")"
ACCEL="-accel tcg,thread=single" CPU=pentium3 NIC="${NIC:--nic none}" ./vm.sh ${MEDIA} > vm.log 2>&1 &
sleep 2; echo "640 400" > souris.pos
