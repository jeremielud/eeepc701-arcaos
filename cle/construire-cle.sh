#!/bin/sh
# construire-cle.sh : fabrique cle-eeepc.img (clé USB amorçable pour l'Eee PC 701)
# à partir de racine/ (Alpine x86 allégé + image ArcaOS) et de ovl/ (configuration).
# Aucun droit root : mtools, syslinux et sfdisk travaillent sur le fichier image.
set -e
cd "$(dirname "$0")"
TAILLE_MIO=940
IMG=cle-eeepc.img
# Clé d'hôte SSH fixe (jamais publiée) : le PC reconnaît la clé d'un démarrage à l'autre.
[ -f ovl/etc/ssh/ssh_host_ed25519_key ] || ssh-keygen -q -t ed25519 -N "" -C eeepc -f ovl/etc/ssh/ssh_host_ed25519_key
tar --owner=0 --group=0 --numeric-owner -czf racine/eeepc.apkovl.tar.gz -C ovl .
rm -f "$IMG"
truncate -s ${TAILLE_MIO}M "$IMG"
echo 'start=2048, type=c, bootable' | /sbin/sfdisk -q "$IMG"
SECTEURS=$(( TAILLE_MIO * 2048 - 2048 ))
mformat -i "$IMG@@1M" -F -v EEEPC -T $SECTEURS -h 255 -s 63 -H 2048 ::
mcopy -s -i "$IMG@@1M" racine/boot racine/apks racine/arcaos racine/.alpine-release racine/eeepc.apkovl.tar.gz ::/
syslinux -t 1048576 -d /boot/syslinux -i "$IMG"
dd if=/usr/lib/syslinux/mbr/mbr.bin of="$IMG" bs=440 count=1 conv=notrunc status=none
mdir -i "$IMG@@1M" ::/
ls -l "$IMG"
