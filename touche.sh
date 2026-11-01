#!/bin/sh
# touche.sh t1 t2 … : envoie des touches (noms QEMU), maintenues 150 ms, 0,3 s d'écart.
cd "$(dirname "$0")"
for k in "$@"; do printf 'sendkey %s 150\n' "$k" | socat - UNIX-CONNECT:mon.sock >/dev/null; sleep 0.3; done
