#!/bin/sh

# Prepare modules directory
# chntpw boot floppy support script
# (c) 2007-2008 Petter N Hagen

MODDIR=/lib/modules/`uname -r`
DRVDIR=/drivers

mkdir -p $MODDIR

echo
echo "** Preparing driver modules to dir $MODDIR"

if [ -f /thisiscd ]
then
  cd $DRVDIR
  mv * $MODDIR
else
  grep nodrivers /proc/cmdline >/dev/null && /scripts/fetchdrv.sh "DISK DRIVERS NEEDED!"
fi

depmod -a

grep nodrivers /proc/cmdline >/dev/null
if [ $? == 1 ]; then
  echo 
  echo "** Will now try to auto-load relevant drivers based on PCI information"
#  sleep 1
  sh /scripts/autoscsi.sh
#  sleep 1
  echo
  echo "** If no disk show up, you may have to try again (d option) or manual (m)."
  echo
  sleep 1
fi

exit 0
