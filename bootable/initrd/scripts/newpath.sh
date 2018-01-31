#
# part.sh (c) 1997-2008 Petter N Hagen
# part of ntchangepasswd bootdisk scripts
#
# Select path registry, and copy to /tmp
#

DSK="/disk"

defroots="windows winnt winnt35"
defpath="windows/system32/config"

pwfiles="sam"
rcfiles="software"
edfiles="system software sam security"

sampath=`cat /tmp/regpath`

cd $DSK/$sampath

echo

(ls -l |egrep -v '(log)|(LOG)|(sav)|(SAV)|(Evt)|(EVT)|(evt)' |more)

while [ $inp"x" != "qx" ]
do
  echo ""
  echo "Select which part of registry to load, use predefined choices"
  echo "or list the files with space as delimiter"
  echo "1 - Password reset [$pwfiles]"
  echo "2 - RecoveryConsole parameters [$rcfiles]"
  echo "3 - Load almost all of it, for regedit tec [$edfiles]"
  echo "q - quit - return to previous"
  read -p "[1] : " inp 
  [ $inp"a" = "a" ] && inp="1"
  case $inp in
      2)  files=$rcfiles
	  inp="q"
	  ;;
      3)  files=$edfiles
	  inp="q"
	  ;;
      1)  files=$pwfiles
	  inp="q"
	  ;;
      [0-9]*) ;;
      "q") exit 1
	  ;;
      *)  files=$inp;
	  inp="q"
	  ;;
  esac
done

echo "Selected files: $files"

echo "Copying $files to /tmp"

unset files2
for f in $files; do
  t=`/scripts/caseglob.awk "$f"`
  e=`echo $t`
  cp $e /tmp || {
    echo "ERROR: Failed to copy registry file $f"
    exit 1
  }
  files2="$files2 $e"
done

echo $files2 >/tmp/files

