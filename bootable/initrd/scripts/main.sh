#!/bin/sh
#
# main.sh (c) 1997-2014 Petter N Hagen
# part of ntchangepasswd bootdisk scripts
#
# Overall control
#

line () {
    echo "========================================================="
}

restart="y"
while [ $restart"q" = "yq" ]; do
echo ""
line
echo "There are several steps to go through:"
echo "- Automatic search for windows installations"
echo "- Select which windows install to change (if more than one)"
echo "- Then finally the password change or registry edit itself"
echo "- If changes were made, write them back to disk"
echo ""
echo "DON'T PANIC! Usually the defaults are OK, just press enter"
echo "             all the way through the questions"
echo ""

line
echo "¤ Step ONE: Select disk partition where the Windows installation is"
line

/scripts/newdisk.sh
nwdr=$?
if [ $nwdr -eq 8 ]; then
	exec /scripts/main-old.sh
fi
if [ $nwdr -eq 0 ]
then

echo
line
echo "¤ Step TWO: Select registry files"
line

/scripts/newpath.sh || continue;

echo
line
echo "¤ Step THREE: Password or registry edit"
line

cd /tmp
files=`cat /tmp/files`

chntpw -L -i $files

rc=$?

if [ $rc -gt 0 -a $rc -ne 2 ]
then
  echo "chntpw failed (returncode $rc)"
  echo "This may be caused by things it does not understand in the registry??"
  echo "Please report this and some of the preceeding messages"
  exit 1
fi

if [ $rc -eq 2 ]
then
  echo
  line
  echo "¤ Step FOUR: Writing back changes"
  line

  rc=1
  while [ $rc -gt 0 ]
  do
    sh /scripts/write.sh
    rc=$?
  done
else
  echo "Registry files not changed, no point in writing it back"
fi

umount /disk 2>/dev/null

echo
echo '***** EDIT COMPLETE *****'
else
echo '* CANCELLED *'
fi
echo
echo "You can try again if it somehow failed, or you selected wrong"
read -p "New run? [n] : " restart

done

umount /disk >/dev/null 2>&1

line
echo
echo "* end of scripts.. returning to the shell.."
echo "* Press CTRL-ALT-DEL to reboot now"
echo "* or do whatever you want from the shell.."
echo "* You may also restart the script procedure with 'sh /scripts/main.sh'"
echo 
exit 0

