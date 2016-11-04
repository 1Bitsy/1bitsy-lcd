##
## This file is part of the libopencm3 project.
##
## Copyright (C) 2009 Uwe Hermann <uwe@hermann-uwe.de>
##
## This library is free software: you can redistribute it and/or modify
## it under the terms of the GNU Lesser General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## This library is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU Lesser General Public License for more details.
##
## You should have received a copy of the GNU Lesser General Public License
## along with this library.  If not, see <http://www.gnu.org/licenses/>.
##

BINARY   := dma-star
SOURCES  := debounce.c gpio.c posix.c systick.c video.c
# BINARY := dda2

OBJS     := $(patsubst %.cpp, %.o, $(SOURCES:%.c=%.o))
LDLIBS   := -lm

LDSCRIPT := stm32f4-1bitsy.ld

V        := 1
CFLAGS :=
include ../Makefile.include
CPPFLAGS += -I.
OPT      := -O3
cflags   := $(filter_out -O%, $(CFLAGS)) -Werror -g
CFLAGS    = $(cflags) $(OPT)

video.o: OPT = -O0

dma-star.o: pixmaps.h

pixmaps.h: extract-pixmaps.py pixmaps/text-snips.png
	python3 extract-pixmaps.py > $@
