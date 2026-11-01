#!/bin/sh
# vm.sh [cd] : démarre la machine d'installation (VNC :5, moniteur dans ./mon.sock).
cd "$(dirname "$0")"
CD=""
ISO=/mnt/data/logiciels/ISO/ArcaOS-5.0.7/ArcaOS-5.0.7.iso
[ "$1" = cd ] && CD="-cdrom $ISO -boot d"
[ "$1" = iso ] && CD="-cdrom $ISO -boot c"
exec qemu-system-i386 ${ACCEL:--enable-kvm} -cpu ${CPU:-host} -no-reboot -smp 1 -m 512 \
  -machine pc \
  -drive file=eeepc701-arcaos.img,format=raw,if=none,id=ssd,cache=writeback \
  -device ide-hd,drive=ssd,bus=ide.0,lcyls=474,lheads=255,lsecs=63,bios-chs-trans=none \
  $CD ${APPORT:+-drive file=apport.img,format=raw,if=floppy} \
  -device intel-hda -device hda-duplex \
  ${NIC:--nic user,model=rtl8139} \
  -vga std -display vnc=127.0.0.1:5 \
  -monitor unix:mon.sock,server,nowait \
  -name eeepc-arcaos ${EXTRA}
