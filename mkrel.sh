#!/bin/sh

make clean
make XC=win clean

make XC=win RELEASE=yes

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
cd iop4
make
cd ../
cd ee
make
mv hdld_svr.elf ../../rel/
cd ../
cd ../

cd rel/
zip -9 hdl_dumx.zip hdl_dum{p,b}.exe hdld_svr.elf
cd ../

# $Id: mkrel.sh,v 1.1 2007-05-12 20:34:46 bobi Exp $
