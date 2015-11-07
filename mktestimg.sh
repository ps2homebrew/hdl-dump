#!/bin/sh
# $Id: mktestimg,v 1.1 2006-09-01 17:37:58 bobi Exp $

# well, maybe fs will need to support sparse files

IMG_NAME=hdd.img
BACKUP=../mine.toc

[ -n "$1" ] && BACKUP="$1"

SIZE=`ls -sk "$BACKUP" | cut "-d " -f1`
SIZE=`echo "$SIZE * 64" | bc`

rm -f $IMG_NAME
dd if=/dev/zero of=$IMG_NAME bs=1048576 seek=$SIZE count=0 2> /dev/null
./hdl_dump restore_toc $IMG_NAME $BACKUP

echo "$IMG_NAME" has been created of virtual size $SIZE KB
