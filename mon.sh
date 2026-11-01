#!/bin/sh
# mon.sh "commande moniteur" : envoie une commande au moniteur QEMU.
cd "$(dirname "$0")"
printf '%s\n' "$1" | socat - UNIX-CONNECT:mon.sock | tail -n +2
