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
make XC=win clean
cd ../

cd rel/
zip -9 hdl_dumx.zip hdl_* boot.elf
cd ../
