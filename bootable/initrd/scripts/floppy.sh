#!/bin/sh

# Enumerate and mount floppies
# chntpw boot floppy support script
# (c) 2004-2008 Petter N Hagen

sel="none"

umount /removable >/dev/null 2>&1

ucnt=`cat /tmp/remparts1 | wc -l | awk '{print $1};'`

ls /dev/fd? | awk -v n=$ucnt '{ n++; printf(" %d : %s\n",n,$1) }' >/tmp/floppies

cat /tmp/remparts1 /tmp/floppies >/tmp/remparts2


fcnt=`cat /tmp/floppies | wc -l | awk '{print $1};'`

cnt=`expr $fcnt + $ucnt`
echo
echo "Found $fcnt floppy drives and $ucnt other removables (USB)"
echo
echo "USB and similar drives:"
cat /tmp/remparts1
echo "Floppy drives:"
cat /tmp/floppies
echo
case $cnt in
    1)
	e=1
	echo "Found only one removable, using it.."
	;;
    0)
	echo "OOPS! Did not find any removable drives"
	exit 1
	;;
    *)
	e="999"
	while [ "0"$e -gt $cnt ];
	do
	  read -p "Please select number from the list, or q to quit: " e
	  [ $e"x" = "qx" ] && exit 1
	done
    ;;
esac

echo "select: $e"

awk "BEGIN {n=1;} {if (n++ == $e) print \$3;}" </tmp/remparts2 >/tmp/remsel

sel=`cat /tmp/remsel`

echo "Selected removable $sel"
echo "Mounting it.."

mount -r -t vfat $sel /removable

echo "Removable selection done.."
echo

