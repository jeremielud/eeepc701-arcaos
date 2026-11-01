#!/bin/sh
# test-vm.sh : démarre la clé dans QEMU (clé en USB, disque interne IDE vierge, réseau
# par câble, SSH de la VM sur le port 2222 de l'hôte, moniteur dans test.sock).
cd "$(dirname "$0")"
exec qemu-system-i386 -enable-kvm -cpu host -m 512 -machine pc \
  -drive file=faux-ssd.img,format=raw,if=ide,index=0 \
  -device usb-ehci,id=ehci -drive file=cle-eeepc.img,format=raw,if=none,id=cle \
  -device usb-storage,bus=ehci.0,drive=cle,bootindex=0 \
  -nic user,model=e1000,hostfwd=tcp:127.0.0.1:2222-:22 \
  -vga std -display vnc=127.0.0.1:6 -monitor unix:test.sock,server,nowait \
  -name cle-eeepc -no-reboot
