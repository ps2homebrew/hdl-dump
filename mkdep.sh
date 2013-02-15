#!/bin/bash

# mkdep.sh - build dependency list for C++ sources
# $Id: mkdep.sh,v 1.2 2004/05/30 20:33:46 b081 Exp $

CC="gcc"
MKDEP="-MM -DNOMINMAX -D_BUILD_WIN32"
DEPENDS="./.depends"

echo > $DEPENDS
for dir in `find -type d`
  do
    list=`ls "$dir/" | grep ".c$"`
    if [ -n "$list" ] ; then
      for src in $dir/*.c;
        do
          echo Building dependency list for $src...
          export RES=`$CC $MKDEP "$src"`
          if [ -n "$RES" ] ; then
            echo -n "$dir/" >> $DEPENDS
	    echo "$RES" >> $DEPENDS
	    echo -ne "\n\n" >> $DEPENDS
          fi
        done
    fi
  done
