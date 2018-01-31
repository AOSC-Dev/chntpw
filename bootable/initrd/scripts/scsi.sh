#!/bin/sh

# List and load SCSI-drivers
# chntpw boot floppy support script
# (c) 2004-2007 Petter N Hagen

DRVDIR=/lib/modules/`uname -r`

loadone () {

    echo "[ $1 ]"
    modprobe $1
    rc=$?
    [ $rc = 0 ] && echo "Driver $1 loaded and initialized."
    return $rc
}


drvr="xx"

# Main loop

echo "==== DISK DRIVER select ===="
while [ $drvr"a" != "qa" ]
do

echo "Disk-drivers currently in cache:"
echo ""
cd $DRVDIR

>/tmp/drivers
>/tmp/drivers.txt
d=1

for f in *.ko *.ko.gz
do
  [ -f $f ] || continue;
  mi=`modinfo -d $f`
  if [ "$miX" == "X" ]; then
     mi=`echo`
  fi
  f=`basename $f .gz`
  f=`basename $f .ko`
  echo $f >>/tmp/drivers
  echo $d | awk '{printf("%3d | %-15s | %s\n",$1,f,mi)}' -v f=$f -v mi="$mi" >>/tmp/drivers.txt
  d=$(($d + 1))
  echo -n .
done
echo

more /tmp/drivers.txt


cd /

echo ""
echo "SCSI driver selection:"
echo "  a - autoprobe for the driver (try all)"
echo "  q - do not load more drivers"
echo "  or enter the number of the desired driver"
echo ""

read -p "SCSI driver select: [q] " drvr params

case $drvr in
    "a")
	l=0
	for d in `cat /tmp/drivers`
	  do
	  loadone $d && {
	      l=1
#	      break
	  }
	done
	if [ $l -eq 0 ]; then
	    echo "All drivers failed load.."
	fi
	;;
    "q"|"")
	echo "No more drivers.."
	drvr="q"
	;;
    [0-9])
        loadone `head -n $drvr /tmp/drivers | tail -1` $params
	;;
    [0-9][0-9])
        loadone `head -n $drvr /tmp/drivers | tail -1` $params
	;;
    [0-9][0-9][0-9])
        loadone `head -n $drvr /tmp/drivers | tail -1` $params
	;;
    *)
	loadone $drvr $params
	;;
esac
done

sleep 1
exit 0
