#!/bin/sh
# installe.sh : relance la machine sur l'ISO, en émulation, sans carte réseau.
cd "$(dirname "$0")"
ACCEL="-accel tcg,thread=single" CPU=pentium3 NIC="-nic none" ./vm.sh cd > vm.log 2>&1 &
sleep 4; printf 'sendkey ret 200\n' | socat - UNIX-CONNECT:mon.sock >/dev/null
echo "640 400" > souris.pos
