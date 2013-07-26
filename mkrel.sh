#!/bin/sh

make clean
make XC=win clean

make XC=win RELEASE=yes

rm -rf rel
mkdir -p rel
mv hdl_dump.exe rel/

make XC=win clean

cd gui
make clean
make XC=win clean
make XC=win RELEASE=yes
mv hdl_dumb.exe ../rel/
upx -9 ../rel/hdl_dumb.exe
make XC=win clean
cd ../

cd svr
make clean
make
mv IOP_PKTDRV.elf ../rel/
ps2-packer ../rel/IOP_PKTDRV.elf ../rel/hdl_svr_093.elf
rm -rf ../rel/IOP_PKTDRV.elf
make clean
cd ../

cd rel/
zip -9 hdl_dumx.zip hdl_*
cd ../

# $Id: mkrel.sh,v 1.1 2007-05-12 20:34:46 bobi Exp $
