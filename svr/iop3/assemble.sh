#!/bin/bash

# part of hdl_dump
# $Id: assemble.sh,v 1.2 2006/05/21 21:47:55 bobi Exp $

if [ ! "`svn --version`" ]; then
  echo 'You need Subversion client (svn).'
  exit
fi

if [ ! "`wget --version`" ]; then
  echo 'You need wget.'
  exit
fi

# get IOP tcpip only from ps2sdk
if [ ! -d tcpip ]; then
  svn checkout svn://svn.ps2dev.org/ps2/trunk/ps2sdk/iop/tcpip/tcpip \
    || { echo "Unable to checkout ps2sdk/tcpip"; exit; }
  cp data/{Defs.make,Rules.make,Rules.release} tcpip/
  patch -p0 -i data/tcpip_lwip-1.1.0.patch
fi

# get lwip-1.1
if [ ! -f lwip-1.1.0.tar.bz2 ]; then
  wget -q http://savannah.nongnu.org/download/lwip/lwip-1.1.0.tar.bz2 \
    || { echo "Unable to download lwip-1.1"; exit; }
fi

if [ ! -d lwip ]; then
  tar xjf lwip-1.1.0.tar.bz2 \
    || { echo "Unable to extract lwip-1.1"; exit; }
  mv lwip-1.1.0 lwip \
    || { echo "Unable to complete lwip-1.1 installation"; exit; }
  patch -p0 -i data/lwip.patch \
    || { echo "Unable to patch lwip-1.1"; exit; }
fi

# at this point tcpip is updated to use lwip-1.1.0
# and most problems are solved, except duplication
# of error constants

# apply final patch
cp data/{hdld_svr.c,hio_iop.c,hio_iop.h} tcpip/src/
patch -p0 -i data/integration.patch
