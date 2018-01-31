#!/bin/sh

# Enumerate disks
# chntpw boot image support script
# (c) 2004-2013 Petter N Hagen

mdev -s 

>/tmp/removables
>/tmp/ntparts2
>/tmp/remparts

# Hack until I can test this on a real machine.
# Do check for CCISS (HP/Compaq DL scsi..)
# since busybox fdisk just looks stuff in /proc/partitions that ends without a digit

d=""

ls /dev | grep -q cciss && d='/dev/cciss!c?d? /dev/sd? /dev/hd?'


# Now find partitions
fdisk -l $d | grep '^Disk' >/tmp/disks
fdisk -l $d | grep '^/dev' >/tmp/partitions
fdisk -l $d | grep '^/dev' |egrep 'NTFS|FAT|SFS' |sed 's/Win95 //g' >/tmp/ntparts3

# Build disk info (top level)
{ while read a dev b; do
  d=`basename $dev | sed 's/://g'`
  r=`cat /sys/block/$d/removable`
  echo -n $a $dev $b >>/tmp/disks2
  if [ ${r}x == "1x" ]; then 
	 echo -n ", REMOVABLE" >>/tmp/disks2
	 echo $d >>/tmp/removables
  fi 
  echo >>/tmp/disks2
 done } < /tmp/disks

 mv /tmp/disks2 /tmp/disks >/dev/null 2>&1

# Build Windows partition info (NTFS, SFS, FAT etc)
  { while read dev b c; do
	  d=`basename $dev | sed 's/[0-9]//g'`
	  r=`cat /sys/block/$d/removable`
	  echo -n "$dev $b $c" >>/tmp/ntparts2
	  if [ "${b}x" == "*x" ]; then 
		 echo -n ", BOOT" >>/tmp/ntparts2
	  fi 
	  if [ ${r}x == "1x" ]; then 
		 echo -n ", REMOVABLE (USB?)" >>/tmp/ntparts2
	  fi 
	  echo >>/tmp/ntparts2
  done } < /tmp/ntparts3

# Build removable partition list
  { while read dev b c; do
	  d=`basename $dev | sed 's/[0-9]//g'`
	  r=`cat /sys/block/$d/removable`
	  if [ ${r}x == "1x" ]; then 
	    echo "$dev $b $c" >>/tmp/remparts
	  fi 
  done } < /tmp/partitions


# Pretty print

# Pretty print table, logic to skip fdisk showing dummy first partition SFS on LDM disk
cat /tmp/ntparts2 |awk 'BEGIN{n=1;} {
  if ( ($6=="SFS" && $2=="1" && $3=="1") ) {	
    prev=$1;
    shft=1;
  } else {
    dev=$1
    if (shft) dev=prev;
    printf("%2d : %20.20s  %6dMB %s%s%s%s%s\n",n++,dev,($2=="*"?$5/1024:$4/1024),$8,$9,$10,$11,$12,$13);
    prev=$1
  }

}' >/tmp/ntparts

# Removables 
cat /tmp/remparts |awk 'BEGIN{n=1;} {printf("%2d : %20.20s  %6dMB %s%s%s%s%s\n",n++,$1,($2=="*"?$5/1024:$4/1024),$8,$9,$10,$11,$12,$13);}' >/tmp/remparts2





# Logic to guess partition with actual windows installation, win 7 onward has a 100MB partition first
# as bootloader, then the main partition, in a default install.
# (instead we should instead try to mount all partitions one by one and look for the registry in each)

cat /tmp/ntparts2 | awk '
        BEGIN{ n=1; part=1; }
	{
		if ($2=="*") {
			size=$5/1024;
               		if (size < 105 && size > 95) part=n+1;  # guess this is boot, so select next
		}

	}
	END { print part; }
	' >/tmp/partdefault

