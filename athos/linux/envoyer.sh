#!/bin/sh
# envoyer.sh : copie les sources sur le 701 et y compile les essais.
cd "$(dirname "$0")/.."
ssh eeepc 'command -v gcc >/dev/null || { apk update -q && apk add -q gcc musl-dev linux-headers make; }; mkdir -p /root/athos'
tar -czf - core compat linux ath.h regd.h regd_common.h reg.h | ssh eeepc 'cd /root/athos && tar -xzf - && cd linux && make -s clean && make -s all 2>&1 | grep -E "error" ; ls -la essai | cut -c1-60'
