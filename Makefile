#
#  Copyright (C) 2013, Zhang YanMing <jamincheung@126.com>
#
#  ingenic log record tool
#
#  This program is free software; you can redistribute it and/or modify it
#  under  the terms of the GNU General  Public License as published by the
#  Free Software Foundation;  either version 2 of the License, or (at your
#  option) any later version.
#
#  You should have received a copy of the GNU General Public License along
#  with this program; if not, write to the Free Software Foundation, Inc.,
#  675 Mass Ave, Cambridge, MA 02139, USA.
#
#

CC = mips-linux-gnu-gcc
AM_CFLAGS = -DDEBUG
CFLAGS ?= -g -O2 -static
CHECKFLAGS = -Wall -Wuninitialized -Wundef
DEPFLAGS = -Wp,-MMD,$(@D)/.$(@F).d,-MT,$@
override CFLAGS := $(CHECKFLAGS) $(AM_CFLAGS) $(CFLAGS)

objects = logwatch.o configure.o

LIBS = -pthread

progs = logwatch

all: $(progs)

.c.o:
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

logwatch: $(objects)
	$(CC) $(CFLAGS) -o $@ $(objects) $(LDFLAGS) $(LIBS)

clean:
	rm -f $(progs) $(objects)

.PHONY: all clean
