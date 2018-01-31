#
# write.rc (c) 1997-2008 Petter N Hagen
# part of ntchangepasswd bootdisk scripts
#
# Write registry files file back
#

read -p "About to write file(s) back! Do it? [n] : " yesno

if [ $yesno"n" = "n" -o $yesno"n" != "yn" ]
then
  echo "No write! Nothing changed!"
  exit 0
fi

fstype=`cat /tmp/fs`
usepart=`cat /tmp/disk`
sampath=`cat /tmp/regpath`
files=`cat /tmp/changed`

for f in $files; do
  echo "Writing " $f
  cpnt /tmp/$f /disk/$sampath/$f
done

umount /disk

exit 0

