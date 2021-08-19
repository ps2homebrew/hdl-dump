#!/bin/sh

make clean

make RELEASE=yes

rm -rf rel
mkdir -p rel
mv hdl_dump.exe rel/

make clean

cd gui
make clean
make RELEASE=yes
mv hdl_dumb.exe ../rel/
make clean
cd ../

cd rel/
zip -9 hdl_dumx.zip hdl_* boot.elf README.txt
cd ../
