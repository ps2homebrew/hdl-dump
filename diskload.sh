#!/bin/sh

# Clone a fresh copy of Open PS2 Loader
rm -rf open-ps2-loader
hg clone https://bitbucket.org/ifcaro/open-ps2-loader || exit 1
cd open-ps2-loader/ || exit 1
# Move the OPL readme across
mv README README_OLD
# Copy in custom stuff
cp -rf ../diskload/* ./
# Make it
make clean
make
