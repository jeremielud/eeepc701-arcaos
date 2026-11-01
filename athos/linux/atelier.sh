#!/bin/sh
# atelier.sh : prépare le 701 (démarré sur la clé) pour les essais : compilateur, sources.
ssh eeepc 'command -v gcc >/dev/null || { apk update -q && apk add -q gcc musl-dev linux-headers make; }; mkdir -p /root/athos'
scp -q "$(dirname "$0")"/*.c "$(dirname "$0")"/*.h eeepc:/root/athos/ 2>/dev/null
