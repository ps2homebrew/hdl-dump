#!/bin/sh

rm -rf open-ps2-loader
hg clone https://bitbucket.org/ifcaro/open-ps2-loader
cd open-ps2-loader/
ls
rm -rf elfldr
rm -rf gfx
rm -rf labs
rm -rf lng
rm -rf pc
rm -rf scripts
rm -rf thirdparty
rm -rf include
rm -rf src
rm -rf modules/cdvd
rm -rf modules/debug
rm -rf modules/mcemu
rm -rf modules/network
rm -rf modules/ps2fs
rm -rf modules/usb
rm -rf modules/vmc
rm -rf modules/wip
rm -rf Makefile
rm -rf .hg
mv README README_OLD
cp -rf ../diskload/* ./
make clean
make
