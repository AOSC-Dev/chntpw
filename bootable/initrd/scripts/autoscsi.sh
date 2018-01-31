#!/bin/sh

# Autoload Disk Drivers
# chntpw boot floppy support script
# (c) 2004-2007 Petter N Hagen

echo
echo "---- AUTO DISK DRIVER select ----"

pcimodules >/tmp/pcidrv

echo "--- PROBE FOUND THE FOLLOWING DRIVERS:"
echo
cat /tmp/pcidrv
echo 
echo "--- TRYING TO LOAD THE DRIVERS"

for m in `cat /tmp/pcidrv`; do
  echo "### Loading $m"
  modprobe ${m}.ko
  echo
done

# Load usb storage and HID if not already loaded
# Not if CD (included in kernel)
[ -f /thisiscd ] || {
  modprobe usbhid.ko
  modprobe usb_storage.ko
}

echo "-------------------------------------------------------------"
echo "Driver load done, if none loaded, you may try manual instead."
echo "-------------------------------------------------------------"
echo

#sleep 1
exit 0

