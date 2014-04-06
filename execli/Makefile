##
## Makefile
## $Id: Makefile,v 1.1 2005/05/06 14:50:35 b081 Exp $
##
## Copyright 2004 Bobi B., w1zard0f07@yahoo.com
##
## This file is part of hdl_dump.
##
## hdl_dump is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.
##
## hdl_dump is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with hdl_dump; if not, write to the Free Software
## Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
##

CFLAGS = -g -Wall -ansi -pedantic -mno-cygwin
EXE = execli.exe
DLL = libexecli.dll

all: $(EXE) $(DLL)

clean:
	rm -f execli_?.o $(EXE) $(DLL)

$(EXE): execli.c
	$(CC) $(CFLAGS) -D_CONSOLE_TEST -c -o execli_e.o execli.c
	$(CC) $(CFLAGS) -o $@ execli_e.o

$(DLL): execli.c execli.def
	$(CC) $(CFLAGS) -c -o execli_l.o execli.c
	$(CC) -shared $(CFLAGS) -o $@ execli_l.o execli.def
