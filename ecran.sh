#!/bin/sh
# ecran.sh nom : capture l'écran de la machine dans ecrans/nom.png
cd "$(dirname "$0")"; mkdir -p ecrans
printf 'screendump %s/ecrans/%s.png -f png\n' "$PWD" "$1" | socat - UNIX-CONNECT:mon.sock >/dev/null
sleep 0.5
