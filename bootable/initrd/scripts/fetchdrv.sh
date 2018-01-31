#!/bin/sh

# Prompt for and merge in drivers from external source (floppy / usb)
# Petter N Hagen, 2007

MODDIR=/lib/modules/`uname -r`

umount /removable >/dev/null 2>&1

echo 
echo $*
echo "You may now insert or swap to the disk drivers floppy or USB key/disk"
read -p "Press enter when done: " dummy
/scripts/floppy.sh
echo "Looking for files named drivers*.zip.."
for f in /removable/drivers*.zip; do
   ls -l $f
   (cd $MODDIR; unzip $f)
   echo "... done"
done

depmod -a

umount /removable >/dev/null 2>&1

