#!/bin/sh
# preparer-racine.sh : prépare racine/ (contenu de la clé USB) à partir de l'ISO Alpine
# x86 « extended » et de l'image ArcaOS (../eeepc701-arcaos.img.gz, non publiée : elle
# contient la licence ArcaOS). Ensuite : ./construire-cle.sh
set -e
cd "$(dirname "$0")"
VERSION=3.24.3
ISO=alpine-extended-$VERSION-x86.iso
[ -f "$ISO" ] || curl -sSfL -O "https://dl-cdn.alpinelinux.org/alpine/v${VERSION%.*}/releases/x86/$ISO"
rm -rf iso racine idx
mkdir -p iso idx racine/apks/x86 racine/arcaos
xorriso -osirrox on -indev "$ISO" -extract / iso >/dev/null 2>&1
chmod -R u+w iso
tar -xzf iso/apks/x86/APKINDEX.tar.gz -C idx
cp -r iso/boot iso/.alpine-release racine/
cp iso/apks/.boot_repository iso/apks/x86/APKINDEX.tar.gz racine/apks/ 2>/dev/null || true
mv racine/apks/APKINDEX.tar.gz racine/apks/x86/
# Seulement les paquets de etc/apk/world et leurs dépendances (12 Mo au lieu de 937).
for f in $(python3 deps.py $(cat ovl/etc/apk/world)); do cp iso/apks/x86/$f racine/apks/x86/; done
rm -rf racine/boot/grub racine/boot/syslinux/isolinux.bin racine/boot/syslinux/boot.cat \
	racine/boot/syslinux/isohdpfx.bin racine/boot/syslinux/*.c32
cp syslinux.cfg racine/boot/syslinux/syslinux.cfg
cp ../eeepc701-arcaos.img.gz ../eeepc701-arcaos.img.sha256 racine/arcaos/
echo "racine/ prête : $(du -sh racine | cut -f1)"
