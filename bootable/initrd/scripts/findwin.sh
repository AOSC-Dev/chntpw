#!/bin/sh

# Go through paritions and find windows on each
# (c) 2013-2014 Petter N Hagen


WINPATH='./*/system32/config'


>/tmp/partitions
>/tmp/disks
>/tmp/pflist
>/tmp/pflistprint

# Skip sr? fd? and disks (not number at end, like sda)

#1 sda1 110413721 105.299
#2 sda5 6802400 6.48727
#3 sdb1 234427488 223.567

tail -n +3 /proc/partitions | awk ' BEGIN { n=1; }
  !/(sr|fd)[0-9]$/ && /[0-9]$/ {
  if ($3 > 10000) printf("%d %s %i %i %i\n",n++,$4,$3,$3/1024/1024,$3/1024);
  }
' >/tmp/partitions

echo "n device bytes   GB  MB === DISK PARTITIONS:"
echo 
cat /tmp/partitions
echo



# Loop on each one to ro mount it

n=1
while read num dev size gb mb; do
  prt="/dev/"${dev}
  echo -n "$mb MB partition $dev "
  if ntfs-3g ${prt} /disk -oro,noatime 2>/dev/tty5    ; then
    ntfs=1
    vfat=0
    echo -n "is NTFS."
  elif mount -t vfat -oro $prt /disk 2>/dev/tty5 ; then
    ntfs=0
    vfat=1
    echo -n "is FAT."
  else
    echo " failed to mount"
    continue    
  fi

  # Disk is mounted, now try to find the registry

  cd /disk
  find . -maxdepth 3 -ipath $WINPATH | sed 's/\.\///' >/tmp/fpath
  if [ -s /tmp/fpath ]; then 
    echo -n " Found windows on: "
    cat /tmp/fpath
    echo /dev/${dev} $ntfs $vfat `cat /tmp/fpath` >>/tmp/pflist
    printf "%2d %-8s %10dMB %s\n" $((n++)) $dev $mb `cat /tmp/fpath` >>/tmp/pflistprint
  else
    echo " No windows there"
  fi
  cd /
  umount /disk
done </tmp/partitions

echo


