#!/bin/sh

cd /home/simon/src/tring

if test $# -gt 0; then
  # Given a hostname on the command line, remotely install to its
  # /mnt/usb. Saves unplugging the USB key!
  lns build/tring tringtmp
  scp tringtmp debugchumby sshkeys excurl timeip "$1":/mnt/usb
  ssh "$1" mv /mnt/usb/tring{tmp,}
  rm tringtmp
  ssh "$1" sync
else
  if mount -t vfat /dev/sdc1 /mnt; then
    cp build/tring /mnt
    cp debugchumby /mnt
    cp sshkeys /mnt
    cp excurl /mnt
    cp timeip /mnt
    umount /mnt
    sync
    sleep 2
    sync
    echo done
  fi
fi
