#!/bin/bash

# part of hdl_dump
# $Id: clean.sh,v 1.1 2005/12/08 20:44:22 bobi Exp $

cd tcpip
make clean
cd ..

TS=`date '+%Y%m%d%H%M%S'`

mkdir -p bak
tar cjf bak/backup-$TS.tar.bz2 tcpip
cp tcpip/src/hdld_svr.c bak/hdld_svr-$TS.c
bzip2 -9 bak/*.c

rm -fR tcpip
rm -fR lwip
