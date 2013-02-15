#!/bin/bash
# test.sh - perform basic tests
# $Id: test.sh,v 1.4 2004/12/04 10:20:52 b081 Exp $

# Beware! This one executes about 1,5-2 hours!

HDL_DUMP_04="/p/ps2/_dump/hdl_dump.exe"
HDL_DUMP_05="/p/ps2/hdl_dump/hdl_dump.exe"
PS2HDD_NEW="hdd2:"
PS2HDD_OLD="hdd2:"
CD_GAME="cd2:"
DVD_GAME="cd3:"
CD_GAME_MD5="2bcd40d8994354c767a52c0fae7b4e96 *-" # rez.iso
DVD_GAME_MD5="5d5064cb45a6957b245520b2a6296876 *-" # ico.iso

TARGET_04="i:"
TARGET_05="i:"
TMP_PREFIX="hdld"

# parameters: $1 - step name, $2 - source (cd1:), $3 - md5 sum
function test_dump ()
{
  echo -n $1...
  $HDL_DUMP_04 dump $2 "$TARGET_04/$TMP_PREFIX.cd.iso" > ./.dummy
  MD5_04=`md5sum < "$TARGET_04/$TMP_PREFIX.cd.iso"`
  rm -f "$TARGET_04/$TMP_PREFIX.cd.iso"
  $HDL_DUMP_05 dump $2 "$TARGET_05/$TMP_PREFIX.cd.iso" > ./.dummy
  MD5_05=`md5sum < "$TARGET_05/$TMP_PREFIX.cd.iso"`
  rm -f "$TARGET_05/$TMP_PREFIX.cd.iso"
  if [ "$MD5_04" = "$MD5_05" -a "$MD5_04" = "$3" ] ; then
    RESULT="ok    "
  else
    RESULT="failed"
    echo \"$MD5_04\" != \"$MD5_05\" != \"$3\"
  fi
  echo -e "\r$1: $RESULT"
}


# parameters: $1 - step name, $2 - cd/dvd, $3 - source (j:\\1.iso), $4 - md5 sum
function test_inject_1 ()
{
  echo -n $1...

  $HDL_DUMP_05 inject_$2 $PS2HDD_NEW "CD" "$3" BLAH_000.00 > ./.dummy
  $HDL_DUMP_04 extract $PS2HDD_OLD "CD" "$TARGET_05/$TMP_PREFIX.cd.iso" > ./.dummy
  MD5_0504=`md5sum < "$TARGET_04/$TMP_PREFIX.cd.iso"`
  NFO_0504=`$HDL_DUMP_04 info $PS2HDD_OLD "CD"`
  rm -f "$TARGET_04/$TMP_PREFIX.cd.iso"
  $HDL_DUMP_05 extract $PS2HDD_NEW "CD" "$TARGET_05/$TMP_PREFIX.cd.iso" > ./.dummy
  MD5_0505=`md5sum < "$TARGET_04/$TMP_PREFIX.cd.iso"`
  NFO_0505=`$HDL_DUMP_05 info $PS2HDD_NEW "CD"`
  rm -f "$TARGET_04/$TMP_PREFIX.cd.iso"
  $HDL_DUMP_05 delete $PS2HDD_NEW "PP.HDL.CD"

  if [ "$MD5_0504" = "$MD5_0505" -a \
       "$MD5_0504" = "$4" ] ; then
    RESULT="ok    "
  else
    RESULT="failed"
    echo \"$MD5_0504\" != \"$MD5_0505\" != \"$4\"
  fi

  echo -e "\r$1: $RESULT"
}


# parameters: $1 - step name, $2 - cd/dvd, $3 - source (j:\\1.iso), $4 - md5 sum
function test_inject_2 ()
{
  echo $1...

  echo -n 1.
  $HDL_DUMP_04 inject_$2 $PS2HDD_OLD "CD" "$3" BLAH_000.00 > ./.dummy
  echo -n 2.
  $HDL_DUMP_04 extract $PS2HDD_OLD "CD" "$TARGET_05/$TMP_PREFIX.cd.iso" > ./.dummy
  echo -n 3.
  MD5_0404=`md5sum < "$TARGET_04/$TMP_PREFIX.cd.iso"`
  echo -n 4.
  NFO_0404=`$HDL_DUMP_04 info $PS2HDD_OLD "CD"`
  echo -n 5.
  rm -f "$TARGET_04/$TMP_PREFIX.cd.iso"
  echo -n 6.
  $HDL_DUMP_05 extract $PS2HDD_NEW "CD" "$TARGET_05/$TMP_PREFIX.cd.iso" > ./.dummy
  echo -n 7.
  MD5_0405=`md5sum < "$TARGET_04/$TMP_PREFIX.cd.iso"`
  echo -n 8.
  NFO_0405=`$HDL_DUMP_05 info $PS2HDD_NEW "CD"`
  echo -n 9.
  rm -f "$TARGET_04/$TMP_PREFIX.cd.iso"
  echo -n 10.
  $HDL_DUMP_04 delete $PS2HDD_OLD "PP.HDL.CD"
  echo -n 11.

  $HDL_DUMP_05 inject_$2 $PS2HDD_NEW "CD" "$3" BLAH_000.00 > ./.dummy
  echo -n 12.
  $HDL_DUMP_04 extract $PS2HDD_OLD "CD" "$TARGET_05/$TMP_PREFIX.cd.iso" > ./.dummy
  echo -n 13.
  MD5_0504=`md5sum < "$TARGET_04/$TMP_PREFIX.cd.iso"`
  echo -n 14.
  NFO_0504=`$HDL_DUMP_04 info $PS2HDD_OLD "CD"`
  echo -n 15.
  rm -f "$TARGET_04/$TMP_PREFIX.cd.iso"
  echo -n 16.
  $HDL_DUMP_05 extract $PS2HDD_NEW "CD" "$TARGET_05/$TMP_PREFIX.cd.iso" > ./.dummy
  echo -n 17.
  MD5_0505=`md5sum < "$TARGET_04/$TMP_PREFIX.cd.iso"`
  echo -n 18.
  NFO_0505=`$HDL_DUMP_05 info $PS2HDD_NEW "CD"`
  echo -n 19.
  rm -f "$TARGET_04/$TMP_PREFIX.cd.iso"
  echo -n 20.
  $HDL_DUMP_05 delete $PS2HDD_NEW "PP.HDL.CD"
  echo -n 21.
  echo .

  if [ "$MD5_0404" = "$MD5_0405" -a \
       "$MD5_0404" = "$MD5_0504" -a \
       "$MD5_0404" = "$MD5_0505" -a \
       "$MD5_0404" = "$4" ] ; then
    RESULT="ok    "
  else
    RESULT="failed"
    echo \"$MD5_0404\" != \"$MD5_0405\" != \"$MD5_0504\" != \"$MD5_0505\" != \"$4\"
  fi

  echo $1: $RESULT
}


for target in "hdd2:" "127.0.0.1"; do

  PS2HDD_NEW="$target"
  PS2HDD_OLD="hdd2:"

  # extract a CD game
  test_dump "dump_cd" $CD_GAME "$CD_GAME_MD5"

  # extract a DVD game
  test_dump "dump_dvd" $DVD_GAME "$DVD_GAME_MD5"

  # install a CD-based game from a optical drive
  test_inject_2 "install_cd_optical" cd $CD_GAME "$CD_GAME_MD5"

  # install a DVD-based game from a optical drive
  test_inject_2 "install_dvd_optical" cd $DVD_GAME "$DVD_GAME_MD5"

  # install a CD-based game from a plain ISO image
  test_inject_2 "install_cd_iso" cd "j:\\ps2\\rez.iso" "$CD_GAME_MD5"

  # install a CD-based game from a Global image
  test_inject_2 "install_cd_gi" cd "j:\\ps2\\rez.gi" "$CD_GAME_MD5"

  # install a CD-based game from a plain Nero image
  test_inject_2 "install_cd_nero_plain" cd "j:\\ps2\\rez_plain.nrg" "$CD_GAME_MD5"

  # install a CD-based game from a RAW Nero image
  test_inject_2 "install_cd_nero_raw" cd "j:\\ps2\\rez_raw.nrg" "$CD_GAME_MD5"

  # install a CD-based game from a plain CDRWIN image
  test_inject_2 "install_cd_cue_plain" cd "j:\\ps2\\rez_plain.cue" "$CD_GAME_MD5"

  # install a CD-based game from a RAW CDRWIN image
  test_inject_2 "install_cd_cue_raw" cd "j:\\ps2\\rez_raw.cue" "$CD_GAME_MD5"

  # install a DVD-based game from a plain ISO image
  test_inject_2 "install_dvd_iso" dvd "j:\\ps2\\ico.iso" "$DVD_GAME_MD5"

  # install a DVD-based game from a Global image
  test_inject_2 "install_dvd_gi" dvd "j:\\ps2\\ico.gi" "$DVD_GAME_MD5"

  # install a DVD-based game from a Nero image
  test_inject_2 "install_dvd_nero" dvd "j:\\ps2\\ico.nrg" "$DVD_GAME_MD5"

done
