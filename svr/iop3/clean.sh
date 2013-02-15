#!/bin/bash

# part of hdl_dump
# $Id: clean.sh,v 1.2 2006/05/21 21:48:00 bobi Exp $

[ ! -d tcpip ] && exit 1

cd tcpip
make clean
cd ..

TS=`date '+%Y%m%d%H%M%S'`

mkdir -p bak
tar cjf bak/backup-$TS.tar.bz2 tcpip
cp tcpip/src/hdld_svr.c bak/hdld_svr-$TS.c
cp tcpip/src/hio_iop.c bak/hio_iop-$TS.c
cp tcpip/src/hio_iop.h bak/hio_iop-$TS.h
bzip2 -9 bak/*.c bak/*.h

rm -fR tcpip
rm -fR lwip
