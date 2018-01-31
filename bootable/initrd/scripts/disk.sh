#!/bin/sh

# Disk partition select
# chntpw boot floppy support script
# (c) 2004-2013 Petter N Hagen

# "rw" for normal, "ro" for debug
RW="rw"

line () {
 echo "========================================================="
}    

umount /disk >/dev/null 2>&1

/scripts/diskscan.sh

echo 
echo "Disks:"
cat /tmp/disks
echo
echo "Candidate Windows partitions found:"
cat /tmp/ntparts

def=`cat /tmp/partdefault`

e="x"

while [ $e"x" != "qx" ];
do
  echo ""
  echo "Please select partition by number or"
  echo " q = quit"
  echo " d = automatically start disk drivers"
  echo " m = manually select disk drivers to load"
  echo " f = fetch additional drivers from floppy / usb"
  echo " a = show all partitions found"
  echo " l = show propbable Windows (NTFS) partitions only"

  echo -n "Select: [$def] "
  read e
  [ $e"x" = "x" ] && e=$def
  case $e in
      "f")
       	  /scripts/fetchdrv.sh "== Additional driver fetch. Swap floppy/usb if needed"
	  echo
	  echo "Now try 'd' or 'm' to try to start the new drivers"
	  echo
	  sleep 1 
	  ;;
      "a")
      	  /scripts/diskscan.sh
	  echo
	  echo "Disks:"
	  cat /tmp/disks
	  echo
	  echo "All partitions:"
	  cat /tmp/partitions
	  ;;
      "l")
      	  /scripts/diskscan.sh
          echo "Candidate Windows partitions found:"
	  cat /tmp/ntparts
	  ;;
      "d")
	  /scripts/autoscsi.sh
      	  /scripts/diskscan.sh
	  echo "Disks:"
	  cat /tmp/disks
	  echo "Candidate Windows partitions found:"
	  cat /tmp/ntparts
	  ;;
      "m")
	  /scripts/scsi.sh
      	  /scripts/diskscan.sh
	  echo "Disks:"
	  cat /tmp/disks
	  echo "Candidate Windows partitions found:"
	  cat /tmp/ntparts
	  ;;
      [0-9]*)
          [ `cat /tmp/ntparts | wc -l` -ge $e ] && {
	  echo
	  echo "Selected $e"
	  echo
	  awk "BEGIN {n=1;} {if (n++ == $e) print \$3;}" </tmp/ntparts >/tmp/disk
	  prt=`cat /tmp/disk`
	  echo -n "Mounting from $prt, with assumed filesystem type "

	  if egrep "^$prt.*(NTFS|SFS)$" /tmp/partitions >/dev/null; then
             fs="ntfs"
	     echo NTFS
	  else
	     fs="vfat"
	     echo "FAT/VFAT/FAT32 and similar"
          fi
	  if [ $fs = "ntfs" ]; then
	     echo "So, let's really check if it is NTFS?"
	     echo
	     ntfs-3g.probe --readwrite $prt
	     nrt=$?
	     flags=""
             if [ $nrt -eq 12 ]; then 
		echo
		echo "Does not seem to be NTFS anyway, trying the FAT variants instead"
		echo
		fs="vfat"
             fi
	     if [ $nrt -eq 14 ]; then
		echo
	        echo "NTFS: Yes, but hibernated"
		line
		echo " ** The system is HIBERNATED!"
		echo " ** SAFEST is to boot into windows and shut down properly!"
		line
		echo
		echo "If that is not possible, you can force changes,"
		echo "but the hibernated session will be lost!"
		echo
	 	read -p "Do you wish to force it? (y/n) [n] " yn
	        if [ $yn"n" = "yn" ]; then
		   nrt=999
		   flags=",remove_hiberfile"
	           echo
		   echo "Your wish is my command, *poof* goes the hibernation"	   
                else
		   echo "No changes made to the disk"
	   	   exit 1
	        fi
	     fi
	     if [ $nrt -eq 15 ]; then
                echo "Yes, but 'dirty'"
		line
		echo " ** The system has not been shut down properly! (is dirty)"
		echo " ** SAFEST is to shut down twice in a row from windows"
		echo " ** then try this again"
		line
		echo
		echo "If that is not possible, you can force changes, but there"
		echo "is a small risk of losing some newly changed files"
		echo
	 	read -p "Do you wish to force it? (y/n) [n] " yn
	        if [ $yn"n" = "yn" ]; then
		   nrt=999
		   flags=",force"
	           echo
		   echo "Using the force.."
                else
		   echo "No changes made to the disk"
	   	   exit 1
	        fi
	     fi
	     if [ $nrt -eq 0 ]; then      # Go for it
	       	echo "Yes, read-write seems OK."
		nrt=999;
             fi	
	     if [ $nrt -eq 999 ]; then
	        echo "Mounting it. This may take up to a few minutes:"
	        ntfs-3g $prt /disk -o $RW,noatime${flags} || {
		     echo
		     echo Failed, returncode $?
		     line
		     echo " ** DID NOT MANAGE TO ACCESS THE PARTITION!"
		     echo " ** Some of the messages above may explain"
		     line
		     read -p "Press return/enter to continue.." yn
		     exit 1
	          }
		echo
		echo "Success!"
	        echo "ntfs" >/tmp/fs
		exit 0
	     fi
	     if [ $nrt -ne 998 ]; then 
	        echo Error
		echo
		echo "NTFS probe returned error code $nrt"
		echo "Sorry, cannot continue"
		echo
		sleep 1
		exit 1
	     fi
	  fi # ntfs check
	  if [ $fs = "vfat" ]; then
	    echo
	    echo "Trying to mount FAT / VFAT / FAT32 etc"
	    echo 
	    mount -t vfat -o$RW,noatime $prt /disk && {
		echo "vfat" >/tmp/fs
		echo
		echo "Success"
		exit 0
	    }
	    echo "ERROR: Mount failed! Try select again or another?"
	  fi
    }
   ;;
  esac
done

exit 1

