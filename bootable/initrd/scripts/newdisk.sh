#!/bin/sh

# Select disk partition to process
# works on results probed by findwin.sh
# part of password reset bootimage
# (c) 2013-2014 Petter N Hagen

RW="rw"

line () {
 echo "========================================================="
}    

umount /disk >/dev/null 2>&1

/scripts/findwin.sh

line
echo
echo "--- Possible windows installations found:"
echo
cat /tmp/pflistprint

e="x"

while [ $e"x" != "qx" ];
do
  echo ""
  echo "Please select partition by number or"
  echo " q = quit.  o = go to old disk select system"
  echo " d = automatically start disk drivers"
  echo " m = manually select disk drivers to load"
  echo " f = fetch additional drivers from floppy / usb"
  echo " a = show all partitions found (fdisk)"
  echo " l = show propbable Windows partitions only"

  echo -n "Select: [1] "
  read e
  [ $e"x" = "x" ] && e="1"
  case $e in
      "o")
	  exit 8
	  ;;
      "f")
       	  /scripts/fetchdrv.sh "== Additional driver fetch. Swap floppy/usb if needed"
	  echo
	  echo "Now try 'd' or 'm' to try to start the new drivers"
	  echo
	  sleep 1 
	  ;;
      "a")
	  echo
	  echo "All partitions:"
	  fdisk -l
	  ;;
      "l")
      	  /scripts/findwin.sh
          echo "Candidate Windows partitions found:"
	  cat /tmp/pflistprint
	  ;;
      "d")
	  /scripts/autoscsi.sh
      	  /scripts/findwin.sh
	  echo "Candidate Windows partitions found:"
	  cat /tmp/pflistprint
	  ;;
      "m")
	  /scripts/scsi.sh
      	  /scripts/findwin.sh
	  echo "Candidate Windows partitions found:"
	  cat /tmp/pflistprint
	  ;;
      [0-9]*)
          [ `cat /tmp/pflist | wc -l` -ge $e ] && {
	  echo
	  echo "Selected $e"
	  echo
	  n=1
	  while read a b c d; do
		  if [ $((n++)) -eq $e ]; then
			  prt=${a}
			  ntfs=${b}
			  vfat=${c}
			  path=${d}
			  continue
		  fi
	  done </tmp/pflist
	  echo $path >/tmp/regpath

	  echo -n "Mounting from $prt, with filesystem type "

	  if [ $ntfs -eq 1 ]; then
		  echo -n "NTFS"
		  fs="ntfs"
	  elif [ $vfat -eq 1 ]; then
	  	  echo -n "VFAT"
		  fs="vfat"
	  else
		  echo -n "unknown??"
	  fi
	  echo

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

